#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include "incbin/incbin.h"

// Macro to embed the default efficiently updatable neural network (NNUE) file
// data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEVALData[];  // a pointer to the embedded data
//     const unsigned char *const gEVALEnd;     // a marker to the end
//     const unsigned int         gEVALSize;    // the size of the embedded file
// Note that this does not work in Microsoft Visual Studio.
#if !defined(_MSC_VER)
INCBIN(EVAL, EVALFILE);
#else
const unsigned char gEVALData[1] = {};
const unsigned char* const gEVALEnd = &gEVALData[1];
const unsigned int gEVALSize = 1;
#endif

Network net;

// Thanks to Disservin for having me look at his code and Luecx for the
// invaluable help and the immense patience

void NNUE::init(const char *file) {
    // open the nn file
    FILE *nn = fopen(file, "rb");

    // if it's not invalid read the config values from it
    if (nn) {
        // initialize an accumulator for every input of the second layer
        size_t read = 0;
        const size_t fileSize = sizeof(Network);
        const size_t objectsExpected = fileSize / sizeof(int16_t);

        read += fread(net.FTWeights, sizeof(int16_t), NUM_INPUTS * L1_SIZE, nn);
        read += fread(net.FTBiases, sizeof(int16_t), L1_SIZE, nn);
        read += fread(net.L1Weights, sizeof(int16_t), L1_SIZE * 2 * OUTPUT_BUCKETS, nn);
        read += fread(net.L1Biases, sizeof(int16_t), OUTPUT_BUCKETS, nn);

        if (read != objectsExpected) {
            std::cout << "Error loading the net, aborting ";
            std::cout << "Expected " << objectsExpected << " shorts, got " << read << "\n";
            exit(1);
        }

        // after reading the config we can close the file
        fclose(nn);
    } else {
        // if we don't find the nnue file we use the net embedded in the exe
        uint64_t memoryIndex = 0;
        std::memcpy(net.FTWeights, &gEVALData[memoryIndex], NUM_INPUTS * L1_SIZE * sizeof(int16_t));
        memoryIndex += NUM_INPUTS * L1_SIZE * sizeof(int16_t);
        std::memcpy(net.FTBiases, &gEVALData[memoryIndex], L1_SIZE * sizeof(int16_t));
        memoryIndex += L1_SIZE * sizeof(int16_t);

        std::memcpy(net.L1Weights, &gEVALData[memoryIndex], L1_SIZE * 2 * OUTPUT_BUCKETS * sizeof(int16_t));
        memoryIndex += L1_SIZE * 2 * OUTPUT_BUCKETS * sizeof(int16_t);
        std::memcpy(net.L1Biases, &gEVALData[memoryIndex], OUTPUT_BUCKETS * sizeof(int16_t));
    }

    int16_t transposedL1Weights[L1_SIZE * 2 * OUTPUT_BUCKETS];
    for (int weight = 0; weight < 2 * L1_SIZE; ++weight)
    {
        for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket)
        {
            const int srcIdx = weight * OUTPUT_BUCKETS + bucket;
            const int dstIdx = bucket * 2 * L1_SIZE + weight;
            transposedL1Weights[dstIdx] = net.L1Weights[srcIdx];
        }
    }
    std::memcpy(net.L1Weights, transposedL1Weights, L1_SIZE * sizeof(int16_t) * 2 * OUTPUT_BUCKETS);
}

void NNUE::update(Accumulator *acc, Position *pos) {
    for (int pov = WHITE; pov <= BLACK; pov++) {
        // Try efficient updates if possible
        bool success = efficientlyUpdatePov(acc, pov);

        // If efficient updates are unsuccessful, then refresh the accumulator
        if (!success) {
            NNUE::Pov_Accumulator &povAccumulator = acc->perspective[pov];
            povAccumulator.accumulate(pos);
            // Reset the add and sub vectors for this accumulator, this will make it "clean" for future updates
            povAccumulator.NNUEAdd.clear();
            povAccumulator.NNUESub.clear();
            // mark any accumulator as refreshed
            povAccumulator.needsRefresh = false;
        }
    }
}

bool NNUE::efficientlyUpdatePov(NNUE::Accumulator *currAcc, int pov) {

    // If our accumulator is clean, no updates are required
    NNUE::Pov_Accumulator &currPovAcc = currAcc->perspective[pov];
    if (currPovAcc.isClean()) return true;

    // If we need to refresh, we cannot perform UE
    if (currPovAcc.needsRefresh) return false;

    // Perform the efficient updates if we can update the previous accumulator
    bool prevSuccess = efficientlyUpdatePov(currAcc - 1, pov);
    if (prevSuccess) {
        NNUE::Pov_Accumulator &prevPovAcc = (currAcc - 1)->perspective[pov];
        currPovAcc.applyUpdate(prevPovAcc);
        return true;
    }

    // One of previous accumulators needed refresh, cannot UE
    return false;
}

void NNUE::Pov_Accumulator::addSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub) {
    const auto Add = &net.FTWeights[add * L1_SIZE];
    const auto Sub = &net.FTWeights[sub * L1_SIZE];
    for (int i = 0; i < L1_SIZE; i++) {
        this->values[i] = prev_acc.values[i] - Sub[i] + Add[i];
    }
}

void NNUE::Pov_Accumulator::addSubSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub1, std::size_t sub2) {
    const auto Add = &net.FTWeights[add * L1_SIZE];
    const auto Sub1 = &net.FTWeights[sub1 * L1_SIZE];
    const auto Sub2 = &net.FTWeights[sub2 * L1_SIZE];
    for (int i = 0; i < L1_SIZE; i++) {
        this->values[i] =  prev_acc.values[i] - Sub1[i] - Sub2[i] + Add[i];
    }
}

int32_t NNUE::ActivateFTAndAffineL1(const int16_t *us, const int16_t *them, const int16_t *weights, const int16_t bias) {
    #if defined(USE_SIMD)
    vepi32 sum  = vec_zero_epi32();
    const vepi16 Zero = vec_zero_epi16();
    const vepi16 One  = vec_set1_epi16(FT_QUANT);
    int weightOffset = 0;
    for (const int16_t *acc : {us, them}) {
        for (int i = 0; i < L1_SIZE; i += CHUNK_SIZE) {
            const vepi16 input   = vec_loadu_epi(reinterpret_cast<const vepi16*>(&acc[i]));
            const vepi16 weight  = vec_loadu_epi(reinterpret_cast<const vepi16*>(&weights[i + weightOffset]));
            const vepi16 clipped = vec_min_epi16(vec_max_epi16(input, Zero), One);

            // In squared clipped relu, we want to do (clipped * clipped) * weight.
            // However, as clipped * clipped does not fit in an int16 while clipped * weight does,
            // we instead do mullo(clipped, weight) and then madd by clipped.
            const vepi32 product = vec_madd_epi16(vec_mullo_epi16(clipped, weight), clipped);
            sum = vec_add_epi32(sum, product);
        }

        weightOffset += L1_SIZE;
    }

    return (vec_reduce_add_epi32(sum) / FT_QUANT + bias) * NET_SCALE / (FT_QUANT * L1_QUANT);

    #else
    int sum = 0;
    int weightOffset = 0;
    for (const int16_t *acc : {us, them}) {
        for (int i = 0; i < L1_SIZE; ++i) {
            int16_t input   = acc[i];
            int16_t weight  = weights[i + weightOffset];
            int16_t clipped = std::clamp(input, int16_t(0), int16_t(FT_QUANT));
            sum += static_cast<int16_t>(clipped * weight) * clipped;
        }

        weightOffset += L1_SIZE;
    }

    return (sum / FT_QUANT + bias) * NET_SCALE / (FT_QUANT * L1_QUANT);
    #endif
}

int32_t NNUE::output(const NNUE::Accumulator& board_accumulator, const int stm, const int outputBucket) {
    // this function takes the net output for the current accumulators and returns the eval of the position
    // according to the net
    const int16_t* us;
    const int16_t* them;

    us = board_accumulator.perspective[stm].values.data();
    them = board_accumulator.perspective[stm ^ 1].values.data();

    const int32_t bucketOffset = 2 * L1_SIZE * outputBucket;
    return ActivateFTAndAffineL1(us, them, &net.L1Weights[bucketOffset], net.L1Biases[outputBucket]);
}

void NNUE::accumulate(NNUE::Accumulator& board_accumulator, Position* pos) {
    for(auto& pov_acc : board_accumulator.perspective) {
        pov_acc.accumulate(pos);
    }
}

void NNUE::Pov_Accumulator::applyUpdate(NNUE::Pov_Accumulator &previousPovAccumulator) {

    assert(previousPovAccumulator.isClean());

    // return early if we already updated this accumulator (aka it's "clean")
    if (this->isClean())
        return;

    // figure out what update we need to apply and do that
    const int adds = NNUEAdd.size();
    const int subs =  NNUESub.size();

    // Quiets
    if (adds == 1 && subs == 1) {
        this->addSub( previousPovAccumulator, this->NNUEAdd[0], this->NNUESub[0]);
    }
    // Captures
    else if (adds == 1 && subs == 2) {
        this->addSubSub(previousPovAccumulator, this->NNUEAdd[0], this->NNUESub[0],this->NNUESub[1]);
    }
    // Castling
    else {
        this->addSub( previousPovAccumulator, this->NNUEAdd[0], this->NNUESub[0]);
        this->addSub(*this, this->NNUEAdd[1], this->NNUESub[1]);
        // Note that for second addSub, we update on top of the half updated current accumulator rather than from scratch
    }

    // Reset the add and sub vectors for this accumulator, this will make it "clean" for future updates
    this->NNUEAdd.clear();
    this->NNUESub.clear();
}

void NNUE::Pov_Accumulator::accumulate(Position *pos) {
    for (int i = 0; i < L1_SIZE; i++) {
       values[i] = net.FTBiases[i];
    }

    const bool flip = get_file[KingSQ(pos, pov)] > 3;

    for (int square = 0; square < 64; square++) {
        const bool input = pos->pieces[square] != EMPTY;
        if (!input) continue;
        const auto Idx = GetIndex(pos->pieces[square], square, flip);
        const auto Add = &net.FTWeights[Idx * L1_SIZE];
        for (int j = 0; j < L1_SIZE; j++) {
            values[j] += Add[j];
        }
    }
}

int NNUE::Pov_Accumulator::GetIndex(const int piece, const int square, bool flip) const {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece];
    auto pieceColorPov = pov == WHITE ? pieceColor : (1 ^ pieceColor);
    // Get the final indexes of the updates, accounting for hm
    auto squarePov = pov == WHITE ? (square ^ 0b111'000) : square;
    if(flip) squarePov ^= 0b000'111;
    std::size_t Idx = pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return Idx;
}
