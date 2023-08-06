#pragma once

#include <cstdint>
#include <array>

constexpr int INPUT_WEIGHTS = 768;
constexpr int HIDDEN_SIZE = 768;

class NNUE {
public:
    using accumulator = std::array<std::array<int16_t, HIDDEN_SIZE>, 2>;

    void init(const char* file);
    void add(NNUE::accumulator& board_accumulator, int piece, int to);
    void clear(NNUE::accumulator& board_accumulator, int piece, int from);
    void move(NNUE::accumulator& board_accumulator, int piece, int from, int to);
    [[nodiscard]] int32_t SCReLU(int16_t x);
    [[nodiscard]] int32_t output(const NNUE::accumulator& board_accumulator, bool whiteToMove);
    void Clear(NNUE::accumulator& board_accumulator);
    [[nodiscard]] std::pair<std::size_t, std::size_t> GetIndex(int piece, int square);

    int16_t featureWeights[INPUT_WEIGHTS * HIDDEN_SIZE];
    int16_t featureBias[HIDDEN_SIZE];
    int16_t outputWeights[HIDDEN_SIZE * 2];
    int16_t outputBias;
};
