#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <immintrin.h>

constexpr int INPUT_WEIGHTS = 768;
constexpr int HIDDEN_SIZE = 1024;

struct Network {
    int16_t featureWeights[INPUT_WEIGHTS * HIDDEN_SIZE];
    int16_t featureBias[HIDDEN_SIZE];
    int16_t outputWeights[HIDDEN_SIZE * 2];
    int16_t outputBias;
};

extern Network net;

class NNUE {
public:
    using accumulator = std::array<std::array<int16_t, HIDDEN_SIZE>, 2>;

    void init(const char* file);
    void add(NNUE::accumulator& board_accumulator, const int piece, const int to);
    void update(NNUE::accumulator& board_accumulator, std::vector<std::pair<std::size_t, std::size_t>>& NNUEAdd, std::vector<std::pair<std::size_t, std::size_t>>& NNUESub);
    void addSub(NNUE::accumulator& board_accumulator, std::size_t whiteAddIdx, std::size_t blackAddIdx, std::size_t whiteSubIdx, std::size_t blackSubIdx);
    void addSubSub(NNUE::accumulator& board_accumulator, std::size_t whiteAddIdx, std::size_t blackAddIdx, std::size_t whiteSubIdx1, std::size_t blackSubIdx1, std::size_t whiteSubIdx2, std::size_t blackSubIdx2);
    [[nodiscard]] int32_t SCReLU(int16_t x);
    [[nodiscard]] int32_t output(const NNUE::accumulator& board_accumulator, const bool whiteToMove);
    [[nodiscard]] std::pair<std::size_t, std::size_t> GetIndex(const int piece, const int square);
    #if defined(USE_AVX2)
    [[nodiscard]] int32_t flatten(const int16_t *acc, const int16_t *weights);
    [[nodiscard]] int32_t horizontal_add(const __m256i sum);
    [[nodiscard]] __m256i simd_screlu(const __m256i vec);
    #elif defined(USE_AVX512)
    [[nodiscard]] int32_t flatten(const int16_t *acc, const int16_t *weights);
    [[nodiscard]] __m512i simd_screlu(const __m512i vec);
    #endif
};
