#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include "simd.h"
#include "types.h"

// Net arch: (768xINPUT_BUCKETS -> L1_SIZE)x2 -> 1xOUTPUT_BUCKETS
constexpr int NUM_INPUTS = 768;
constexpr int INPUT_BUCKETS = 16;
constexpr int L1_SIZE = 1536;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT = 255;
constexpr int L1_QUANT = 64;
constexpr int NET_SCALE = 400;
//
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

struct Network {
    int16_t FTWeights[INPUT_BUCKETS * NUM_INPUTS * L1_SIZE];
    int16_t FTBiases[L1_SIZE];
    int16_t L1Weights[L1_SIZE * 2 * OUTPUT_BUCKETS];
    int16_t L1Biases[OUTPUT_BUCKETS];
};

extern Network net;
struct Position;

struct NNUE {

    using PovAccumulator = std::array<int16_t, L1_SIZE>;

    struct FinnyTableEntry {
        Bitboard occupancies[12] = {};
        NNUE::PovAccumulator accumCache;

        FinnyTableEntry() {
            for (int i = 0; i < L1_SIZE; ++i)
                accumCache[i] = net.FTBiases[i];
        }
    };

    using FinnyTable = std::array<std::array<std::array<FinnyTableEntry, 2>, INPUT_BUCKETS>, 2>;

    static int activateAffine(Position *pos, FinnyTable* FinnyPointer, const int16_t *weights, const int16_t bias);
    static int povActivateAffine(Position *pos, FinnyTable* FinnyPointer, const int side, const int16_t *l1weights);
    static int output(Position *pos, FinnyTable* FinnyPointer);
    static void init(const char *file);
    static size_t getIndex(const int piece, const int square, const int side, const int bucket, const bool flip);
};