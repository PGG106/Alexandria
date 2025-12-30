#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdint>
#include <cstring>
#include "incbin/incbin.h"
#include <fstream>
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

UnquantisedNetwork unquantisedNet;
QuantisedNetwork quantisedNet;

void load_unquantize_andquant() {
    // open the nn file
    std::ifstream stream{"nn.net", std::ios::binary};

    stream.read(reinterpret_cast<char *>(&unquantisedNet), sizeof(UnquantisedNetwork));

    // Quantise FT Weights
    for (int i = 0; i < INPUT_BUCKETS * NUM_INPUTS * L1_SIZE; ++i)
        quantisedNet.FTWeights[i] = static_cast<int16_t>(std::round(unquantisedNet.FTWeights[i] * FT_QUANT));

    // Quantise FT Biases
    for (int i = 0; i < L1_SIZE; ++i)
        quantisedNet.FTBiases[i] = static_cast<int16_t>(std::round(unquantisedNet.FTBiases[i] * FT_QUANT));

    // Quantise L1, L2 and L3 weights and biases
    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket) {

        // Quantise L1 Weights
        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                quantisedNet.L1Weights[i][bucket][j] = static_cast<int8_t>(std::round(unquantisedNet.L1Weights[i][bucket][j] * L1_QUANT));

        // Quantise L1 Biases and factoriser
        for (int i = 0; i < L2_SIZE; ++i) {
            float bias = unquantisedNet.L1Biases[bucket][i];
            for (int f = 0; f < L1_SIZE; ++f)
                bias += unquantisedNet.Factoriser[f] * unquantisedNet.L1Weights[f][bucket][i];
            quantisedNet.L1Biases[bucket][i] = bias;
        }

        // Quantise L2 Weights
        for (int i = 0; i < L2_SIZE; ++i)
            for (int j = 0; j < L3_SIZE; ++j)
                quantisedNet.L2Weights[i][bucket][j] = unquantisedNet.L2Weights[i][bucket][j];

        // Quantise L2 Biases
        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L2Biases[bucket][i] = unquantisedNet.L2Biases[bucket][i];

        // Quantise L3 Weights
        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L3Weights[i][bucket] = unquantisedNet.L3Weights[i][bucket];

        // Quantise L3 Biases
        quantisedNet.L3Biases[bucket] = unquantisedNet.L3Biases[bucket];
    }

    std::ofstream out{"quantNet.nn", std::ios::binary};
    out.write(reinterpret_cast<const char *>(&quantisedNet), sizeof(QuantisedNetwork));
}

void NNUE::init() {

    // load embedded prequanted net
    quantisedNet = *reinterpret_cast<const QuantisedNetwork*>(gEVALData);

    // Transform the quantised weights and biases into the form we want for optimal inference
    // FT Weights
    for (int i = 0; i < INPUT_BUCKETS * NUM_INPUTS * L1_SIZE; ++i)
        net.FTWeights[i] = quantisedNet.FTWeights[i];

    // FT Biases
    for (int i = 0; i < L1_SIZE; ++i)
        net.FTBiases[i] = quantisedNet.FTBiases[i];


    // Transpose L1, L2 and L3 weights and biases
    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket) {

        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                net.L1Weights[bucket][j * L1_SIZE + i] = quantisedNet.L1Weights[i][bucket][j];

        // Transpose L1 Biases
        for (int i = 0; i < L2_SIZE; ++i)
            net.L1Biases[bucket][i] = quantisedNet.L1Biases[bucket][i];

        // Transpose L2 Weights
        for (int i = 0; i < L2_SIZE; ++i)
            for (int j = 0; j < L3_SIZE; ++j)
                 net.L2Weights[bucket][i * L3_SIZE + j] = quantisedNet.L2Weights[i][bucket][j];

        // Transpose L2 Biases
        for (int i = 0; i < L3_SIZE; ++i)
            net.L2Biases[bucket][i] = quantisedNet.L2Biases[bucket][i];

        // Transpose L3 Weights
        for (int i = 0; i < L3_SIZE; ++i)
             net.L3Weights[bucket][i] = quantisedNet.L3Weights[i][bucket];

        // Transpose L3 Biases
         net.L3Biases[bucket] = quantisedNet.L3Biases[bucket];
    }
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
            accumCache[j] += net.FTWeights[added + j];
        }
    }

      for (size_t i = 0; i < removeCnt; i++) {
        const auto removed = remove[i];
        for (int j = 0; j < L1_SIZE; ++j) {
            accumCache[j] -= net.FTWeights[removed + j];
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

    propagateL1(FTOutputs, net.L1Weights[outputBucket], net.L1Biases[outputBucket], L1Outputs);

    propagateL2(L1Outputs,net.L2Weights[outputBucket], net.L2Biases[outputBucket], L2Outputs );

    propagateL3(L2Outputs, net.L3Weights[outputBucket],  net.L3Biases[outputBucket], L3Output);

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
