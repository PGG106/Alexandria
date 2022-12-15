#pragma once
#include <cstdint>
#include <vector>
#include <array>

#define INPUT_WEIGHTS 768
#define HIDDEN_BIAS 512
#define HIDDEN_WEIGHTS 512
#define OUTPUT_BIAS 1

class NNUE {
public:
	using accumulator = std::array<int16_t, HIDDEN_BIAS>;

	void init(const char* nn);
	void add(NNUE::accumulator& accumulator,int piece, int to);
	void clear(NNUE::accumulator& accumulator,int piece, int from);
	void move(NNUE::accumulator& accumulator,int piece, int from, int to);
	int relu(int x);
	int32_t output(const NNUE::accumulator& accumulator);
	void Clear(NNUE::accumulator& accumulator);

	
	uint8_t inputValues[INPUT_WEIGHTS];
	int16_t inputWeights[INPUT_WEIGHTS * HIDDEN_WEIGHTS];
	int16_t hiddenBias[HIDDEN_BIAS];
	int16_t hiddenWeights[HIDDEN_WEIGHTS];
	int32_t outputBias[OUTPUT_BIAS];
};