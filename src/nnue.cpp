#include "nnue.h"
#include <cstdio>
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin/incbin.h"
#include "Board.h"




int NNUE::relu(int x)
{

	return std::max(0, x);
}

void NNUE::init(const char* file)
{
	//initialize an accumulator for every input of the second layer


	//open the nn file 
	FILE* nn = fopen(file, "rb");

	//if it's not invalid read the config values from it 
	if (nn) {

		fread(inputWeights, sizeof(int16_t), INPUT_WEIGHTS * HIDDEN_WEIGHTS, nn);
		fread(hiddenBias, sizeof(int16_t), HIDDEN_BIAS, nn);
		fread(hiddenWeights, sizeof(int16_t), HIDDEN_WEIGHTS, nn);
		fread(outputBias, sizeof(int32_t), OUTPUT_BIAS, nn);

		//after reading the config we can close the file
		fclose(nn);
	}
	else {
		exit(100);
	}
}

void NNUE::activate(int inputNum)
{
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		accumulator[i] += inputWeights[inputNum * HIDDEN_BIAS + i];
	}


}

void NNUE::deactivate(int inputNum)
{
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		accumulator[i] -= inputWeights[inputNum * HIDDEN_BIAS + i];
	}
}

int32_t NNUE::output()
{
	int32_t output = 0;
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		output += relu(accumulator[i]) * hiddenWeights[i];
	}
	output += outputBias[0];
	return output / (64 * 256);
}

void NNUE::Clear()
{


	for (int i = 0; i < HIDDEN_BIAS; i++) {
		accumulator[i] = 0;
	}

}

