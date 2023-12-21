#pragma once

#include <cstdint>
#include <array>

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
    void clear(NNUE::accumulator& board_accumulator, const int piece, const int from);
    void move(NNUE::accumulator& board_accumulator, const int piece, const int from, const int to);
    [[nodiscard]] int32_t SCReLU(int16_t x);
    [[nodiscard]] int32_t output(const NNUE::accumulator& board_accumulator, const bool whiteToMove);
    void Clear(NNUE::accumulator& board_accumulator);
    [[nodiscard]] std::pair<std::size_t, std::size_t> GetIndex(const int piece, const int square);
};
