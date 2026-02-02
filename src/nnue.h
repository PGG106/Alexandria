#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include <cmath>
#include "simd.h"
#include "types.h"

// Net arch: (768xINPUT_BUCKETS -> L1_SIZE)x2 ->16-> 32 -> 1xOUTPUT_BUCKETS
constexpr int NUM_INPUTS = 768;
constexpr int INPUT_BUCKETS = 16;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT  = 255;
constexpr int L1_QUANT  = 64;
constexpr int FT_SHIFT  = 10;
constexpr int NET_SCALE = 362;

constexpr bool MERGE_KING_PLANES = false;

constexpr float L1_MUL  = float(1 << FT_SHIFT) / float(FT_QUANT * FT_QUANT * L1_QUANT);
constexpr float WEIGHT_CLIPPING = 1.98f;
static_assert(std::round(L1_QUANT * WEIGHT_CLIPPING) * (FT_QUANT * FT_QUANT >> FT_SHIFT) * 4 <= 32767);

#if defined(USE_SIMD)
constexpr int FT_CHUNK_SIZE = sizeof(vepi16) / sizeof(int16_t);
constexpr int L1_CHUNK_SIZE = sizeof(vepi8 ) / sizeof(int8_t);
constexpr int L2_CHUNK_SIZE = sizeof(vps32 ) / sizeof(float);
constexpr int L3_CHUNK_SIZE = sizeof(vps32 ) / sizeof(float);
constexpr int L1_CHUNK_PER_32 = sizeof(int32_t) / sizeof(int8_t);
#else
constexpr int L1_CHUNK_PER_32 = 1;
#endif

constexpr int buckets[64] = {
         0,  1,  2,  3,  3,  2,  1, 0,
         4,  5,  6,  7,  7,  6,  5, 4,
         8,  9, 10, 11, 11, 10,  9, 8,
         8,  9, 10, 11, 11, 10,  9, 8,
        12, 12, 13, 13, 13, 13, 12, 12,
        12, 12, 13, 13, 13, 13, 12, 12,
        14, 14, 15, 15, 15, 15, 14, 14,
        14, 14, 15, 15, 15, 15, 14, 14
};

[[nodiscard]] inline int getBucket(int kingSquare, int side) {
   const auto finalKingSq = side == WHITE ? (kingSquare ^ 56) : (kingSquare);
   return buckets[finalKingSq];
}

using NNUEIndices = std::array<std::size_t, 2>;

struct UnquantisedNetwork {
    float Factoriser[NUM_INPUTS * L1_SIZE];
    float FTWeights[INPUT_BUCKETS * NUM_INPUTS * L1_SIZE];
    float FTBiases[L1_SIZE];
    float L1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float L1Biases[OUTPUT_BUCKETS][L2_SIZE];
    float L2Weights[L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float L2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float L3Biases[OUTPUT_BUCKETS];
};

struct QuantisedNetwork {
    int16_t FTWeights[INPUT_BUCKETS * NUM_INPUTS * L1_SIZE];
    int16_t FTBiases [L1_SIZE];
    int8_t  L1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float   L1Biases [OUTPUT_BUCKETS][L2_SIZE];
    float   L2Weights[L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float   L2Biases [OUTPUT_BUCKETS][L3_SIZE];
    float   L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float   L3Biases [OUTPUT_BUCKETS];
};


struct Network {
    alignas(64) int16_t FTWeights[INPUT_BUCKETS * NUM_INPUTS * L1_SIZE];
    alignas(64) int16_t FTBiases [L1_SIZE];
    alignas(64) int8_t  L1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    alignas(64) float   L1Biases [OUTPUT_BUCKETS][L2_SIZE];
    alignas(64) float   L2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
    alignas(64) float   L2Biases [OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float   L3Weights[OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float   L3Biases [OUTPUT_BUCKETS];
};

extern const Network* net;
struct Position;

struct NNUE {

    using PovAccumulator = std::array<int16_t, L1_SIZE>;

    struct alignas(64) FinnyTableEntry {
        NNUE::PovAccumulator accumCache;
        Bitboard occupancies[12] = {};

        FinnyTableEntry() {
            for (int i = 0; i < L1_SIZE; ++i)
                accumCache[i] = net->FTBiases[i];
        }
    };

    using FinnyTable = std::array<std::array<std::array<FinnyTableEntry, 2>, INPUT_BUCKETS>, 2>;

    static void activateAffine(Position *pos, FinnyTable* FinnyPointer,  uint8_t *output);
    static void povActivateAffine(Position *pos, FinnyTable* FinnyPointer, const int side,  uint8_t *output);

    static void propagateL1(const uint8_t *inputs, const int8_t *weights, const float *biases, float *output);
    static void propagateL2(const float *inputs, const float *weights, const float *biases, float *output);
    static void propagateL3(const float *inputs, const float *weights, const float bias, float &output);

    static int output(Position *pos, FinnyTable* FinnyPointer);
    static void init();
    static size_t getIndex(const int piece, const int square, const int side, const int bucket, const bool flip);
};