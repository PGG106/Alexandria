#include "nnue.h"
#include "Board.h"
#include <cstdio>
#include <cstring>
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin/incbin.h"
#if !defined(_MSC_VER)
INCBIN(EVAL, EVALFILE);
#else
const unsigned char gEVALData[1] = { 0x0 };
const unsigned char* const gEmbeddedNNUEEnd = &gEVALData[1];
const unsigned int gEmbeddedNNUESize = 1;
#endif

// Thanks to Disservin for having me look at his code and Lucex for the
// invaluable help and the immense patience

std::vector<std::array<int16_t, HIDDEN_BIAS>> accumulatorStack;
std::vector<std::array<int16_t, HIDDEN_BIAS>> accumulatorStackBlack;

int NNUE::relu(int x) { return std::max(0, x); }

void NNUE::init(const char* file) {
	// initialize an Accumulator for every input of the second layer

	// open the nn file
	FILE* nn = fopen(file, "rb");

	// if it's not invalid read the config values from it
	if (nn) {

		fread(inputWeights, sizeof(int16_t), INPUT_WEIGHTS * HIDDEN_WEIGHTS, nn);
		fread(hiddenBias, sizeof(int16_t), HIDDEN_BIAS, nn);
		fread(hiddenWeights, sizeof(int16_t), HIDDEN_WEIGHTS, nn);
		fread(outputBias, sizeof(int32_t), OUTPUT_BIAS, nn);
		// after reading the config we can close the file
		fclose(nn);
	}
	else {
		//if we don't find the nnue file we use the net embedded in the exe
		int memoryIndex = 0;
		std::memcpy(inputWeights, &gEVALData[memoryIndex],
			INPUT_WEIGHTS * HIDDEN_WEIGHTS * sizeof(int16_t));
		memoryIndex += INPUT_WEIGHTS * HIDDEN_WEIGHTS * sizeof(int16_t);
		std::memcpy(hiddenBias, &gEVALData[memoryIndex],
			HIDDEN_BIAS * sizeof(int16_t));
		memoryIndex += HIDDEN_BIAS * sizeof(int16_t);

		std::memcpy(hiddenWeights, &gEVALData[memoryIndex],
			HIDDEN_WEIGHTS * sizeof(int16_t));
		memoryIndex += HIDDEN_WEIGHTS * sizeof(int16_t);
		std::memcpy(outputBias, &gEVALData[memoryIndex], 1 * sizeof(int32_t));
		memoryIndex += OUTPUT_BIAS * sizeof(int32_t);
	}
}

void NNUE::activate(int piece, int to, int stm) {
	int piecetype = piece % 6;
	int whiteIndex = to + piecetype * 64 + (Color[piece] != WHITE) * 64 * 6;
	int blackIndex = (to ^ 56) + piecetype * 64 + (Color[piece] != BLACK) * 64 * 6;

	for (int i = 0; i < HIDDEN_BIAS; i++) {
		whiteAccumulator[i] += inputWeights[whiteIndex * HIDDEN_BIAS + i];
		blackAccumulator[i] += inputWeights[blackIndex * HIDDEN_BIAS + i];
	}
}


void NNUE::deactivate(int piece, int to, int stm) {
	int piecetype = piece % 6;
	int whiteIndex = to + piecetype * 64 + (Color[piece] != WHITE) * 64 * 6;
	int blackIndex = (to ^ 56) + piecetype * 64 + (Color[piece] != BLACK) * 64 * 6;

	for (int i = 0; i < HIDDEN_BIAS; i++) {
		whiteAccumulator[i] -= inputWeights[whiteIndex * HIDDEN_BIAS + i];
		blackAccumulator[i] -= inputWeights[blackIndex * HIDDEN_BIAS + i];
	}
}

int32_t NNUE::output(int stm) {
	//this function takes the net output for the current accumulators and returns the eval of the position according to the net
	int32_t output = 0;
	if (stm == WHITE) {
		for (int i = 0; i < HIDDEN_BIAS; i++) {
			output += relu(whiteAccumulator[i]) * hiddenWeights[i];
		}
		output += outputBias[0];
	}
	else {
		for (int i = 0; i < HIDDEN_BIAS; i++) {
			output += relu(blackAccumulator[i]) * hiddenWeights[i];
		}
		output += outputBias[0];
	}
	return output / (64 * 256);
}

void NNUE::Clear() {
	//Reset the accumulators of the nnue
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		whiteAccumulator[i] = 0;
	}
}
