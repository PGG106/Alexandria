#pragma once
#include <cstdint>
#include <vector>
#include <array>

#define INPUT_WEIGHTS 64 * 12
#define HIDDEN_BIAS 64 * 8
#define HIDDEN_WEIGHTS 64 * 8
#define OUTPUT_BIAS 1

class NNUE {
public:
	void init(const char* nn);
	void activate(int piece, int to, int stm);
	void deactivate(int piece, int to, int stm);
	int relu(int x);
	int32_t output(int stm);
	void Clear();

	std::array<int16_t, HIDDEN_BIAS> whiteAccumulator;
	std::array<int16_t, HIDDEN_BIAS> blackAccumulator;
	uint8_t inputValues[INPUT_WEIGHTS];
	int16_t inputWeights[INPUT_WEIGHTS * HIDDEN_WEIGHTS];
	int16_t hiddenBias[HIDDEN_BIAS];
	int16_t hiddenWeights[HIDDEN_WEIGHTS];
	int32_t outputBias[OUTPUT_BIAS];
};