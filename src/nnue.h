#pragma once
#include <vector>
#include <cstdint>

#define INPUT_WEIGHTS 64 * 12
#define HIDDEN_BIAS 64 * 2
#define HIDDEN_WEIGHTS 64 * 2
#define OUTPUT_BIAS 1


class NNUE {
public:
	void init(const char* nn);
	void activate(int inputNum);
	void deactivate(int inputNum);
	int relu(int x);
	int32_t output();
	void Clear();

	int16_t accumulator[HIDDEN_BIAS];
	uint8_t inputValues[INPUT_WEIGHTS];
	int16_t inputWeights[INPUT_WEIGHTS * HIDDEN_WEIGHTS];
	int16_t hiddenBias[HIDDEN_BIAS];
	int16_t hiddenWeights[HIDDEN_WEIGHTS];
	int32_t outputBias[OUTPUT_BIAS];
};