#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

int NNUE::povActivateAffine(Position *pos, NNUE::FinnyTable* FinnyPointer,  const int side, const int16_t *l1weights) {

    #if defined(USE_SIMD)
    constexpr int NUM_REGI = 8;
    constexpr int TILE_SIZE = NUM_REGI * sizeof(vepi16) / sizeof(int16_t);
    const vepi16 Zero = vec_zero_epi16();
    const vepi16 One  = vec_set1_epi16(FT_QUANT);
    vepi16 regs[NUM_REGI];
    vepi32 sum = vec_zero_epi32();
    #else
    int sum = 0;
    #endif

    const int kingSq = KingSQ(pos, side);
    const bool flip = get_file[kingSq] > 3;
    const int kingBucket = getBucket(kingSq, side);
    FinnyTableEntry &cachedEntry = (*FinnyPointer)[side][kingBucket][flip];


    size_t add[32], remove[32]; // Max add or remove is 32 unless illegal position
    size_t addCnt = 0, removeCnt = 0;

    for (int piece = WP; piece <= BK; piece++) {
        Bitboard added = pos->bitboards[piece] & ~cachedEntry.occupancies[piece];
        Bitboard removed = cachedEntry.occupancies[piece] & ~pos->bitboards[piece];
        while (added) {
            int square = popLsb(added);
            add[addCnt++] = getIndex(piece, square, side, kingBucket, flip);
        }

        while (removed) {
            int square = popLsb(removed);
            remove[removeCnt++] = getIndex(piece, square, side, kingBucket, flip);
        }

        cachedEntry.occupancies[piece] = pos->bitboards[piece];
    }

    #if defined(USE_SIMD)
    for (int i = 0; i < L1_SIZE; i += TILE_SIZE) {
        vepi16 *entryVec = reinterpret_cast<vepi16*>(&cachedEntry.accumCache[i]);
        for (int j = 0; j < NUM_REGI; ++j) {
            regs[j] = vec_loadu_epi(&entryVec[j]);
        }

        for (size_t j = 0; j < addCnt; ++j) {
            vepi16 *addedVec = reinterpret_cast<vepi16*>(&net.FTWeights[add[j] * L1_SIZE + i]);
            for (int k = 0; k < NUM_REGI; ++k) {
                regs[k] = vec_add_epi16(regs[k], addedVec[k]);
            }
        }

        for (size_t j = 0; j < removeCnt; ++j) {
            vepi16 *removedVec = reinterpret_cast<vepi16*>(&net.FTWeights[remove[j] * L1_SIZE + i]);
            for (int k = 0; k < NUM_REGI; ++k) {
                regs[k] = vec_sub_epi16(regs[k], removedVec[k]);
            }
        }

        for (int j = 0; j < NUM_REGI; ++j) {
            vec_storeu_epi(&entryVec[j], regs[j]);
        }

        const vepi16 *l1weightVec = reinterpret_cast<const vepi16*>(&l1weights[i]);
        for (int j = 0; j < NUM_REGI; ++j) {

            // We have Squared Clipped ReLU (SCReLU) as our activation function.
            // We first clip the input (the CReLU part)
            regs[j] = vec_min_epi16(vec_max_epi16(regs[j], Zero), One);

            // Load the weight
            const vepi16 weight = vec_loadu_epi(&l1weightVec[j]);

            // What we want to do here is square regs[j] (aka CReLU) to achieve SCReLU and then multiply by the weight.
            // However, as regs[j] * regs[j] does not fit in an int16 while regs[j] * weight does,
            // we instead do mullo(regs[j], weight) and then madd by regs[j].
            const vepi32 product = vec_madd_epi16(vec_mullo_epi16(regs[j], weight), regs[j]);
            sum = vec_add_epi32(sum, product);
        }
    }

    return vec_reduce_add_epi32(sum);
    #else

    NNUE::PovAccumulator &accumCache = cachedEntry.accumCache;

      for (int i = 0; i < addCnt; i++) {
        const auto added = add[i];
        for (int i = 0; i < L1_SIZE; ++i) {
            accumCache[i] += net.FTWeights[added * L1_SIZE + i];
        }
    }

      for (int i = 0; i < removeCnt; i++) {
        const auto removed = remove[i];
        for (int i = 0; i < L1_SIZE; ++i) {
            accumCache[i] -= net.FTWeights[removed * L1_SIZE + i];
        }
    }

    for (int i = 0; i < L1_SIZE; ++i) {
        const int16_t input   = accumCache[i];
        const int16_t weight  = l1weights[i];
        const int16_t clipped = std::clamp(input, int16_t(0), int16_t(FT_QUANT));
        sum += static_cast<int16_t>(clipped * weight) * clipped;
    }

    return sum;
    #endif
}

int NNUE::activateAffine(Position *pos, NNUE::FinnyTable* FinnyPointer, const int16_t *weights, const int16_t bias) {
    int sum =    povActivateAffine(pos, FinnyPointer, pos->side    , &weights[0])
               + povActivateAffine(pos, FinnyPointer, pos->side ^ 1, &weights[L1_SIZE]);

    return (sum / FT_QUANT + bias) * NET_SCALE / (FT_QUANT * L1_QUANT);
}

int NNUE::output(Position *pos, NNUE::FinnyTable* FinnyPointer) {
    const int pieceCount = pos->PieceCount();
    const int outputBucket = std::min((63 - pieceCount) * (32 - pieceCount) / 225, 7);
    const int32_t bucketOffset = 2 * L1_SIZE * outputBucket;

    return activateAffine(pos, FinnyPointer, &net.L1Weights[bucketOffset], net.L1Biases[outputBucket]);
}

size_t NNUE::getIndex(const int piece, const int square, const int side, const int bucket, const bool flip) {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece];
    const int pieceColorPov = pieceColor ^ side;

    // Get the final indexes of the updates, accounting for hm
    auto squarePov = side == WHITE ? (square ^ 0b111'000) : square;
    if (flip) squarePov ^= 0b000'111;
    return bucket * NUM_INPUTS + pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
}
