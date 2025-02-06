#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include "incbin/incbin.h"
#include "io.h"

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

        read += fread(net.FTWeights, sizeof(int16_t), INPUT_BUCKETS * NUM_INPUTS * L1_SIZE, nn);
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
        std::memcpy(net.FTWeights, &gEVALData[memoryIndex], INPUT_BUCKETS * NUM_INPUTS * L1_SIZE * sizeof(int16_t));
        memoryIndex += INPUT_BUCKETS * NUM_INPUTS * L1_SIZE * sizeof(int16_t);
        std::memcpy(net.FTBiases, &gEVALData[memoryIndex], L1_SIZE * sizeof(int16_t));
        memoryIndex += L1_SIZE * sizeof(int16_t);

        std::memcpy(net.L1Weights, &gEVALData[memoryIndex], L1_SIZE * 2 * OUTPUT_BUCKETS * sizeof(int16_t));
        memoryIndex += L1_SIZE * 2 * OUTPUT_BUCKETS * sizeof(int16_t);
        std::memcpy(net.L1Biases, &gEVALData[memoryIndex], OUTPUT_BUCKETS * sizeof(int16_t));
    }
}

void NNUE::update(Accumulator *acc, Position *pos) {
    for (int pov = WHITE; pov <= BLACK; pov++) {
        auto &povAccumulator = (acc)->perspective[pov];
        auto &previousPovAccumulator = (acc - 1)->perspective[pov];

        // return early if we already updated this accumulator (aka it's "clean")
        if (povAccumulator.isClean())
            continue;
        // if the top accumulator needs a refresh, ignore all the other dirty accumulators and just refresh it
        else if (povAccumulator.needsRefresh) {
            povAccumulator.refresh(pos);
            continue;
        }
        // if the previous accumulator is clean, just UE on top of it and avoid the need to scan backwards
        else if (previousPovAccumulator.isClean()) {
            povAccumulator.applyUpdate(previousPovAccumulator);
            continue;
        }

        const auto Acc = [&](const int j) -> Pov_Accumulator& {
            return pos->accumStack[j].perspective[pov];
        };

        // if we can't update we need to start scanning backwards
        // if in our scan we'll find an accumulator that needs a refresh we'll just refresh the top one, otherwise we'll start an UE chain
        for (int UEableAccs = 1; UEableAccs < MAXPLY; UEableAccs++) {
            auto &currentAcc = (acc - UEableAccs)->perspective[pov];
            // check if the current acc should be refreshed
            if (currentAcc.needsRefresh) {
                povAccumulator.refresh(pos);
                break;
            }
            // If not check if it's a valid starting point for an UE chain
            else if ((acc - UEableAccs - 1)->perspective[pov].isClean()) {
                for (int j = (pos->accumStackHead - 1 - UEableAccs); j <= pos->accumStackHead - 1; j++) {
                    Acc(j).applyUpdate(Acc(j-1));
                }
                break;
            }
        }
    }
}

void NNUE::Pov_Accumulator::refresh(Position *pos) {
    // probe the finny table for a cached state
    const auto kingSq = KingSQ(pos, pov);
    const bool flip = get_file[KingSQ(pos, pov)] > 3;
    const int kingBucket = getBucket(kingSq, pov);
    FinnyTableEntry* cachedEntry = &pos->FTable[pov].Table[kingBucket][flip];
    this->values = cachedEntry->accumCache.perspective[pov].values;

    if(cachedEntry->occupancies[WK] == 0ULL)
        for (int i = 0; i < L1_SIZE; i++) {
            this->values[i] = net.FTBiases[i];
        }

    // figure out a diff
    for (int piece = WP; piece <= BK; piece++) {
        auto added = pos->bitboards[piece] & ~cachedEntry->occupancies[piece];
        auto removed = cachedEntry->occupancies[piece] & ~pos->bitboards[piece];
        while (added && removed) {
            auto added_square = popLsb(added);
            auto added_index = GetIndex(piece, added_square, kingSq, flip);
            const auto Add = &net.FTWeights[added_index * L1_SIZE];
            auto removed_square = popLsb(removed);
            auto removed_index = GetIndex(piece, removed_square, kingSq, flip);
            const auto Sub = &net.FTWeights[removed_index * L1_SIZE];
            for (int i = 0; i < L1_SIZE; i++) {
              this->values[i] = this->values[i] + Add[i] - Sub[i];
            }
        }
        while (added) {
            auto square = popLsb(added);
            auto index = GetIndex(piece, square, kingSq, flip);
            const auto Add = &net.FTWeights[index * L1_SIZE];
            for (int i = 0; i < L1_SIZE; i++) {
                this->values[i] += Add[i];
            }
        }
        while (removed) {
            auto square = popLsb(removed);
            auto index = GetIndex(piece, square, kingSq, flip);
            const auto Sub = &net.FTWeights[index * L1_SIZE];
            for (int i = 0; i < L1_SIZE; i++) {
                this->values[i] -= Sub[i];
            }
        }
    }

    // Reset the add and sub vectors for this accumulator, this will make it "clean" for future updates
    this->NNUEAdd.clear();
    this->NNUESub.clear();
    // mark any accumulator as refreshed
    this->needsRefresh = false;
    // store the refreshed finny table entry value
    std::memcpy( &cachedEntry->accumCache.perspective[pov].values,  &values, sizeof(values));
    std::memcpy( cachedEntry->occupancies,  pos->bitboards, sizeof(Bitboard) * 12);
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
            const int16_t input   = acc[i];
            const int16_t weight  = weights[i + weightOffset];
            const int16_t clipped = std::clamp(input, int16_t(0), int16_t(FT_QUANT));
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

void NNUE::refresh(NNUE::Accumulator& board_accumulator, Position* pos) {
    for(auto& pov_acc : board_accumulator.perspective) {
        pov_acc.refresh(pos);
    }
}

void NNUE::Pov_Accumulator::applyUpdate(NNUE::Pov_Accumulator& previousPovAccumulator) {

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
        // Note that for second addSub, we put acc instead of acc - 1 because we are updating on top of
        // the half-updated accumulator
    }

    // Reset the add and sub vectors for this accumulator, this will make it "clean" for future updates
    this->NNUEAdd.clear();
    this->NNUESub.clear();
}

int NNUE::Pov_Accumulator::GetIndex(const int piece, const int square, const int kingSq, bool flip) const {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece];
    auto pieceColorPov = pov == WHITE ? pieceColor : (pieceColor ^ 1);
    // Get the final indexes of the updates, accounting for hm
    auto squarePov = pov == WHITE ? (square ^ 0b111'000) : square;
    if(flip) squarePov ^= 0b000'111;
    const int bucket = getBucket(kingSq, pov);
    std::size_t Idx = bucket * NUM_INPUTS + pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return Idx;
}
