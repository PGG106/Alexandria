#pragma once

#include <cstdint>
#include <array>
#include <vector>

#if defined(USE_AVX512) || defined(USE_AVX2)
#include <immintrin.h>
#endif

constexpr int INPUT_WEIGHTS = 768;
constexpr int HIDDEN_SIZE = 1536;
constexpr int OUTPUT_BUCKETS = 8;

using NNUEIndices = std::pair<std::size_t, std::size_t>;

struct Network {
    int16_t featureWeights[INPUT_WEIGHTS * HIDDEN_SIZE];
    int16_t featureBias[HIDDEN_SIZE];
    int16_t outputWeights[HIDDEN_SIZE * 2 * OUTPUT_BUCKETS];
    int16_t outputBias[OUTPUT_BUCKETS];
};

extern Network net;
struct Position;

class NNUE {
public:
    struct Accumulator {
        std::array<std::array<int16_t, HIDDEN_SIZE>, 2> values;
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
    [[nodiscard]] int32_t flatten(const int16_t *acc, const int16_t *weights);
    [[nodiscard]] int32_t output(const NNUE::Accumulator &board_accumulator, const bool whiteToMove, const int outputBucket);
    [[nodiscard]] NNUEIndices GetIndex(const int piece, const int square);
    #if defined(USE_AVX2)
    [[nodiscard]] int32_t horizontal_add(const __m256i sum);
    #endif
};
