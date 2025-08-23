#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdint>
#include <cstring>
#include "io.h"

// Macro to embed the default efficiently updatable neural network (NNUE) file
// data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEVALData[];  // a pointer to the embedded data
//     const unsigned char *const gEVALEnd;     // a marker to the end
//     const unsigned int         gEVALSize;    // the size of the embedded file
// Note that this does not work in Microsoft Visual Studio.
#if defined(__cpp_pp_embed)
const unsigned char gEVALData[] = {
#embed EVALFILE
};
const unsigned int gEVALDataSize = sizeof(gEVALData) / sizeof(gEVALData[0]);
const unsigned char* const gEVALEnd = &gEVALData[gEVALDataSize];
#elif ! defined(_MSC_VER)
#include "incbin/incbin.h"
INCBIN(EVAL, EVALFILE);
#else
const unsigned char gEVALData[1] = {};
const unsigned char* const gEVALEnd = &gEVALData[1];
const unsigned int gEVALSize = 1;
#endif

const Network *net;

// Thanks to Disservin for having me look at his code and Luecx for the
// invaluable help and the immense patience

void NNUE::init() {
    net = reinterpret_cast<const Network*>(gEVALData);
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
        Bitboard added = pos->state().bitboards[piece] & ~cachedEntry.occupancies[piece];
        Bitboard removed = cachedEntry.occupancies[piece] & ~pos->state().bitboards[piece];
        while (added) {
            int square = popLsb(added);
            add[addCnt++] = getIndex(piece, square, side, kingBucket, flip);
        }

        while (removed) {
            int square = popLsb(removed);
            remove[removeCnt++] = getIndex(piece, square, side, kingBucket, flip);
        }

        cachedEntry.occupancies[piece] = pos->state().bitboards[piece];
    }

    #if defined(USE_SIMD)
    for (int i = 0; i < L1_SIZE; i += TILE_SIZE) {
        vepi16 *entryVec = reinterpret_cast<vepi16*>(&cachedEntry.accumCache[i]);
        for (int j = 0; j < NUM_REGI; ++j) {
            regs[j] = vec_loadu_epi(&entryVec[j]);
        }

        for (size_t j = 0; j < addCnt; ++j) {
            const auto *addedVec = reinterpret_cast<const vepi16*>(&net->FTWeights[add[j] + i]);
            for (int k = 0; k < NUM_REGI; ++k) {
                regs[k] = vec_add_epi16(regs[k], addedVec[k]);
            }
        }

        for (size_t j = 0; j < removeCnt; ++j) {
            const auto *removedVec = reinterpret_cast<const vepi16*>(&net->FTWeights[remove[j] + i]);
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

      for (size_t i = 0; i < addCnt; i++) {
        const auto added = add[i];
        for (int j = 0; j < L1_SIZE; ++j) {
            accumCache[j] += net->FTWeights[added + j];
        }
    }

      for (size_t i = 0; i < removeCnt; i++) {
        const auto removed = remove[i];
        for (int j = 0; j < L1_SIZE; ++j) {
            accumCache[j] -= net->FTWeights[removed + j];
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

    return activateAffine(pos, FinnyPointer, &net->L1Weights[bucketOffset], net->L1Biases[outputBucket]);
}

size_t NNUE::getIndex(const int piece, const int square, const int side, const int bucket, const bool flip) {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece];
    const int pieceColorPov = pieceColor ^ side;

    // Get the final indexes of the updates, accounting for hm
    auto squarePov = square ^ (0b111'000 * !side) ^ (0b000'111 * flip);
    auto idx = bucket * NUM_INPUTS + pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return idx * L1_SIZE;
}
