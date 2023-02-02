#pragma once
#include <cstdint>
#include <vector>
#include <array>

constexpr int INPUT_WEIGHTS = 768;
constexpr int HIDDEN_BIAS = 512;
constexpr int HIDDEN_WEIGHTS = 512;
constexpr int OUTPUT_BIAS = 1;

class NNUE {
public:
	using accumulator = std::array<int16_t, HIDDEN_BIAS>;

	void init(const char* nn);
	void add(NNUE::accumulator& board_accumulator, int piece, int to);
	void clear(NNUE::accumulator& board_accumulator, int piece, int from);
	void move(NNUE::accumulator& board_accumulator, int piece, int from, int to);
	int relu(int x);
	int32_t output(const NNUE::accumulator& board_accumulator);
	void Clear(NNUE::accumulator& board_accumulator);
	int GetIndex(int piece, int square);

	int16_t inputWeights[INPUT_WEIGHTS * HIDDEN_WEIGHTS];
	int16_t hiddenBias[HIDDEN_BIAS];
	int16_t hiddenWeights[HIDDEN_WEIGHTS];
	int32_t outputBias[OUTPUT_BIAS];
};