#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include "simd.h"

// Net arch: (768 -> L1_SIZE)x2 -> 1xOUTPUT_BUCKETS
constexpr int NUM_INPUTS = 768;
constexpr int L1_SIZE = 1536;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT  = 255;
constexpr int L1_QUANT  = 64;
constexpr int NET_SCALE = 400;

#if defined(USE_SIMD)
constexpr int CHUNK_SIZE = sizeof(vepi16) / sizeof(int16_t);
#else
constexpr int CHUNK_SIZE = 1;
#endif

using NNUEIndices = std::pair<std::size_t, std::size_t>;

struct Network {
    int16_t FTWeights[NUM_INPUTS * L1_SIZE];
    int16_t FTBiases [L1_SIZE];
    int16_t L1Weights[L1_SIZE * 2 * OUTPUT_BUCKETS];
    int16_t L1Biases [OUTPUT_BUCKETS];
};

extern Network net;
struct Position;

class NNUE {
public:
    struct Accumulator {
        std::array<std::array<int16_t, L1_SIZE>, 2> values;
        std::vector<NNUEIndices> NNUEAdd = {};
        std::vector<NNUEIndices> NNUESub = {};

        void AppendAddIndex(NNUEIndices index) {
            NNUEAdd.emplace_back(index);
        }

        void AppendSubIndex(NNUEIndices index) {
            NNUESub.emplace_back(index);
        }
    };

    void init(const char *file);
    void accumulate(NNUE::Accumulator &board_accumulator, Position* pos);
    void update(NNUE::Accumulator *acc);
    void addSub(NNUE::Accumulator *new_acc, NNUE::Accumulator *prev_acc, NNUEIndices add, NNUEIndices sub);
    void addSubSub(NNUE::Accumulator *new_acc, NNUE::Accumulator *prev_acc, NNUEIndices add, NNUEIndices sub1, NNUEIndices sub2);
    [[nodiscard]] int32_t ActivateFTAndAffineL1(const int16_t *us, const int16_t *them, const int16_t *weights, const int16_t bias);
    [[nodiscard]] int32_t output(const NNUE::Accumulator &board_accumulator, const bool whiteToMove, const int outputBucket);
    [[nodiscard]] NNUEIndices GetIndex(const int piece, const int square);
};
