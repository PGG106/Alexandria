#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include <cmath>

#include "bitboard.h"
#include "simd.h"
#include "types.h"

struct Position;

struct SquarePiece {
    Square square = no_sq;
    int piece = EMPTY;
};

struct DirtyPieces {
    std::array<SquarePiece, 2> removed{};
    std::array<SquarePiece, 2> added{};
    uint8_t removedCount = 0;
    uint8_t addedCount = 0;

    void remove(const Square square, const int piece) {
        assert(removedCount < removed.size());
        removed[removedCount++] = {square, piece};
    }

    void add(const Square square, const int piece) {
        assert(addedCount < added.size());
        added[addedCount++] = {square, piece};
    }
};

// Net arch: (768xINPUT_BUCKETS -> L1_SIZE)x2 ->16-> 32 -> 1xOUTPUT_BUCKETS
constexpr bool MERGE_KING_PLANES = false;
constexpr bool DUAL_ACTIVATION = true;
constexpr int NUM_INPUTS = 768;
constexpr int INPUT_BUCKETS = 16;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int EFFECTIVE_L2_SIZE = 16 * (1 + DUAL_ACTIVATION);
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT  = 255;
constexpr int L1_QUANT  = 64;
constexpr int FT_SHIFT  = 10;
constexpr int NET_SCALE = 362;

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
    float L2Weights[EFFECTIVE_L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float L2Biases[OUTPUT_BUCKETS][L3_SIZE];
    float L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float L3Biases[OUTPUT_BUCKETS];
};

struct QuantisedNetwork {
    int16_t FTWeights[INPUT_BUCKETS * NUM_INPUTS * L1_SIZE];
    int16_t FTBiases [L1_SIZE];
    int8_t  L1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float   L1Biases [OUTPUT_BUCKETS][L2_SIZE];
    float   L2Weights[EFFECTIVE_L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float   L2Biases [OUTPUT_BUCKETS][L3_SIZE];
    float   L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float   L3Biases [OUTPUT_BUCKETS];
};


struct Network {
    int16_t FTWeights[INPUT_BUCKETS * NUM_INPUTS * L1_SIZE];
    int16_t FTBiases [L1_SIZE];
    int8_t  L1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    float   L1Biases [OUTPUT_BUCKETS][L2_SIZE];
    float   L2Weights[OUTPUT_BUCKETS][EFFECTIVE_L2_SIZE * L3_SIZE];
    float   L2Biases [OUTPUT_BUCKETS][L3_SIZE];
    float   L3Weights[OUTPUT_BUCKETS][L3_SIZE];
    float   L3Biases [OUTPUT_BUCKETS];
};

extern const Network* net;

struct NNUE {

    using PovAccumulator = std::array<int16_t, L1_SIZE>;

    struct alignas(64) Accumulator {
        std::array<PovAccumulator, 2> colors{};
        std::array<bool, 2> updated{};
        std::array<Square, 2> kings{no_sq, no_sq};
        DirtyPieces dirtyPieces{};
    };

    struct AccumulatorStack {
        std::array<Accumulator, MAXPLY + 1> entries{};
        int head = 0;

        void reset(Position* pos);

        Accumulator& push() {
            assert(head < MAXPLY);
            Accumulator& accumulator = entries[++head];
            accumulator.updated = {false, false};
            return accumulator;
        }

        void pop() {
            assert(head > 0);
            --head;
        }

        Accumulator& current() {
            return entries[head];
        }
    };

    struct alignas(64) FinnyTableEntry {
        NNUE::PovAccumulator accumCache;
        Bitboard occupancies[12] = {};

        FinnyTableEntry() {
            for (int i = 0; i < L1_SIZE; ++i)
                accumCache[i] = net->FTBiases[i];
        }
    };

    using FinnyTable = std::array<std::array<std::array<FinnyTableEntry, 2>, INPUT_BUCKETS>, 2>;

    static void activateAffine(Position *pos, FinnyTable *FinnyPointer, AccumulatorStack* accumulatorStack,
                               uint16_t *base, uint16_t *nnzIndices, int &nnzCount, uint8_t *output);
    static void povActivateAffine(Position *pos, FinnyTable *FinnyPointer, AccumulatorStack* accumulatorStack,
                                  int side, uint16_t *base, uint16_t *nnzIndices, int &nnzCount, uint8_t *output);

    static void propagateL1(const uint8_t *inputs, uint16_t *nnzIndices, int nnzCount, const int8_t *weights, const float *biases, float *output);
    static void propagateL2(const float *inputs, const float *weights, const float *biases, float *output);
    static void propagateL3(const float *inputs, const float *weights, const float bias, float &output);

    static int output(Position *pos, FinnyTable* FinnyPointer, AccumulatorStack* accumulatorStack);
    static void init();
    static size_t getIndex(const int piece, const int square, const int side, const int bucket, const bool flip);
};

// NNZTable stores all the possible 8-bit combinations, active indices and active indices count
struct NNZEntry {
    uint16_t indices[8];
    int      count;
};

struct NNZTable {
    NNZEntry table[256];

    NNZTable() {
        for (uint16_t i = 0; i <= 255; ++i) {
            this->table[i].count = CountBits(i);
            Bitboard j = i;
            uint16_t k = 0;
            while (j)
                this->table[i].indices[k++] = popLsb(j);
        }
    };
};

extern NNZTable nnzTable;