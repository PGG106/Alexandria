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
    enum Type : uint8_t {
        NONE,
        NORMAL,
        CAPTURE,
        CASTLING
    } type = NONE;

    SquarePiece sub0, add0, sub1, add1;
};

// Net arch: (768xINPUT_BUCKETS -> L1_SIZE)x2 ->16-> 32 -> 1xOUTPUT_BUCKETS
constexpr int NUM_INPUTS = 768;
constexpr int INPUT_BUCKETS = 13;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int EFFECTIVE_L2_SIZE = 32;
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT  = 255;
constexpr int L1_QUANT  = 128;
constexpr int FT_SHIFT  = 9;
constexpr int NET_SCALE = 400;

constexpr float L1_MUL = 1.0f / float(FT_QUANT * FT_QUANT * L1_QUANT >> FT_SHIFT);

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
     8,  8,  9,  9,  9,  9,  8, 8,
    10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12
};

[[nodiscard]] inline int getBucket(int kingSquare, int side) {
   const auto finalKingSq = side == WHITE ? (kingSquare ^ 56) : (kingSquare);
   return buckets[finalKingSq];
}

using NNUEIndices = std::array<std::size_t, 2>;

struct Network {
    alignas(64) int16_t FTWeights[INPUT_BUCKETS][2][6][64][L1_SIZE];
    alignas(64) int16_t FTBiases[L1_SIZE];
    union {
        alignas(64) int8_t L1Weights[OUTPUT_BUCKETS][L1_SIZE][L2_SIZE];
        alignas(64) int8_t L1WeightsAlt[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    };
    alignas(64) float L1Biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(64) float L2Weights[OUTPUT_BUCKETS][EFFECTIVE_L2_SIZE][L3_SIZE];
    alignas(64) float L2Biases[OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float L3Weights[OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float L3Biases[OUTPUT_BUCKETS];
};

static_assert(sizeof(Network) == 30905920);

extern const Network* net;

struct NNUE {

    using PovAccumulator = std::array<int16_t, L1_SIZE>;

    struct alignas(64) Accumulator {
        PovAccumulator colors[2];
        bool updated[2] = {};
        Square kings[2] = {no_sq, no_sq};
        DirtyPieces dirtyPieces;
        ZobristKey key = 0;

        void addPiece(Square kingSquare, int side, int piece, Square square);
        void movePiece(Square kingSquare, int side, int piece, Square from, Square to);
        void removePiece(Square kingSquare, int side, int piece, Square square);
        void doUpdates(Square kingSquare, int side, const Accumulator& input);
        void reset(int side);
        void refresh(Position* pos, int side);
    };

    struct alignas(64) FinnyEntry {
        Bitboard occupancies[2][12] = {};
        Accumulator accumulator;

        void reset();
    };

    struct FinnyTable {
        Accumulator accumulatorStack[MAXPLY + 1];
        FinnyEntry entries[2][INPUT_BUCKETS];
        ZobristKey rootKey = 0;
        bool initialized = false;

        void reset();
        Accumulator& prepare(Position* pos);
        void refresh(Position* pos, Accumulator& accumulator, int side);
        void update(Position* pos, Accumulator& accumulator, int head);
    };

    static bool needRefresh(int side, Square oldKing, Square newKing);
    static void activateAffine(const Accumulator& accumulator, int sideToMove, uint16_t *base, uint16_t *nnzIndices, int &nnzCount, uint8_t *output);
    static void povActivateAffine(const Accumulator& accumulator, int side, uint16_t *base, uint16_t *nnzIndices, int &nnzCount, uint8_t *output);

    static void propagateL1(const uint8_t *inputs, uint16_t *nnzIndices, int nnzCount, const int8_t *weights, const float *biases, float *output);
    static void propagateL2(const float *inputs, const float *weights, const float *biases, float *output);
    static void propagateL3(const float *inputs, const float *weights, const float bias, float &output);

    static int output(Position *pos, FinnyTable* FinnyPointer);
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