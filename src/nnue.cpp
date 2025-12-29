#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdint>
#include <cstring>
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

const Network *net;

// Thanks to Disservin for having me look at his code and Luecx for the
// invaluable help and the immense patience

void NNUE::init() {
    net = reinterpret_cast<const Network*>(gEVALData);
}

// does FT activate for one pov at a time
void NNUE::povActivateAffine(Position *pos, NNUE::FinnyTable* FinnyPointer,  const int side, uint8_t *output) {

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

    for (int i = 0; i < L1_SIZE / 2; ++i) {
        int16_t clipped0 = std::clamp<int16_t>(accumCache[i], 0, FT_QUANT);
        int16_t clipped1 = std::clamp<int16_t>(accumCache[i + L1_SIZE / 2], 0, FT_QUANT);
        output[i] = static_cast<uint8_t>(clipped0 * clipped1 >> FT_SHIFT);
    }
}

void NNUE::propagateL1(const uint8_t *inputs, const int8_t *weights, const float *biases, float *output) {
    int sums[L2_SIZE] = {};
    for (int i = 0; i < L1_SIZE; ++i) {
        for (int j = 0; j < L2_SIZE; ++j) {
            sums[j] += static_cast<int32_t>(inputs[i] * weights[j * L1_SIZE + i]);
        }
    }

    for (int i = 0; i < L2_SIZE; ++i) {
        // Convert into floats and activate L1
        const float clipped = std::clamp(float(sums[i]) * L1_MUL + biases[i], 0.0f, 1.0f);
        const float squared = clipped * clipped;
        output[i] = squared;
    }
}

void NNUE::propagateL2(const float *inputs, const float *weights, const float *biases, float *output) {
    float sums[L3_SIZE];

    for (int i = 0; i < L3_SIZE; ++i)
        sums[i] = biases[i];

    // Affine transform for L2
    for (int i = 0; i < L2_SIZE; ++i) {
        const float *weight = &weights[i * L3_SIZE];
        for (int out = 0; out < L3_SIZE; ++out) {
            sums[out] += inputs[i] * weight[out];
        }
    }

    // Activate L2
    for (int i = 0; i < L3_SIZE; ++i) {
        const float clipped = std::clamp(sums[i], 0.0f, 1.0f);
        const float squared = clipped * clipped;
        output[i] = squared;
    }
}

inline float reduce_add(float *sums, const int length) {
    if (length == 2) return sums[0] + sums[1];
    for (int i = 0; i < length / 2; ++i)
        sums[i] += sums[i + length / 2];

    return reduce_add(sums, length / 2);
}

void NNUE::propagateL3(const float *inputs, const float *weights, const float bias, float &output) {
    constexpr int avx512chunk = 512 / 32;
    constexpr int numSums = avx512chunk;
    float sums[numSums] = {};

    // Affine transform for L3
    for (int i = 0; i < L3_SIZE; ++i) {
        sums[i % numSums] += inputs[i] * weights[i];
    }
    output = reduce_add(sums, numSums) + bias;
}

void NNUE::activateAffine(Position *pos, NNUE::FinnyTable* FinnyPointer,  uint8_t *output) {
    povActivateAffine(pos, FinnyPointer, pos->side,  output);
    povActivateAffine(pos, FinnyPointer, pos->side ^ 1,  &output[L1_SIZE / 2]);
}

int NNUE::output(Position *pos, NNUE::FinnyTable* FinnyPointer) {
    const int pieceCount = pos->PieceCount();
    const int outputBucket = std::min((63 - pieceCount) * (32 - pieceCount) / 225, 7);
    alignas (64) uint8_t  FTOutputs[L1_SIZE];
    alignas (64) float    L1Outputs[L2_SIZE];
    alignas (64) float    L2Outputs[L3_SIZE];
    float L3Output;

    // does FT activation for both accumulators
    activateAffine(pos, FinnyPointer, FTOutputs);

    for (auto partial : FTOutputs) {if (partial != 0)  std::cout << static_cast<int32_t>(partial) << " "; }

    propagateL1(FTOutputs, net->L1Weights[outputBucket], net->L1Biases[outputBucket], L1Outputs);

    propagateL2(L1Outputs,net->L2Weights[outputBucket], net->L2Biases[outputBucket], L2Outputs );

    propagateL3(L2Outputs, net->L3Weights[outputBucket], net->L3Biases[outputBucket], L3Output);

    return L3Output * NET_SCALE;
}

size_t NNUE::getIndex(const int piece, const int square, const int side, const int bucket, const bool flip) {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece] * (!MERGE_KING_PLANES || piecetype == KING);
    const int pieceColorPov = pieceColor ^ side;

    // Get the final indexes of the updates, accounting for hm
    auto squarePov = square ^ (0b111'000 * !side) ^ (0b000'111 * flip);
    auto idx = bucket * NUM_INPUTS + pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return idx * L1_SIZE;
}
