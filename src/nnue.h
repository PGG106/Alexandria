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
	void init(const char* nn);
	void add(int piece, int to);
	void clear(int piece, int from);
	void move(int piece, int from, int to);
	int relu(int x);
	int32_t output();
	void Clear();

	std::array<int16_t, HIDDEN_BIAS> accumulator;
	uint8_t inputValues[INPUT_WEIGHTS];
	int16_t inputWeights[INPUT_WEIGHTS * HIDDEN_WEIGHTS];
	int16_t hiddenBias[HIDDEN_BIAS];
	int16_t hiddenWeights[HIDDEN_WEIGHTS];
	int32_t outputBias[OUTPUT_BIAS];
};