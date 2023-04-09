#include "nnue.h"
#include "board.h"
#include <cstdio>
#include <cstring>
#include <iostream>
// Macro to embed the default efficiently updatable neural network (NNUE) file
// data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEVALData[];  // a pointer to the embedded data
//     const unsigned char *const gEVALEnd;     // a marker to the end
//     const unsigned int         gEVALSize;    // the size of the embedded file
// Note that this does not work in Microsoft Visual Studio.
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin/incbin.h"
#if !defined(_MSC_VER)
INCBIN(EVAL, EVALFILE);
#else
const unsigned char gEVALData[1] = { 0x0 };
const unsigned char* const gEVALEnd = &gEVALData[1];
const unsigned int gEVALSize = 1;
#endif

// Thanks to Disservin for having me look at his code and Lucex for the
// invaluable help and the immense patience

int NNUE::relu(int x) { return std::max(0, x); }

void NNUE::init(const char* file) {
	// initialize an accumulator for every input of the second layer
	size_t read = 0;
	size_t fileSize = INPUT_WEIGHTS * HIDDEN_WEIGHTS + HIDDEN_BIAS + HIDDEN_WEIGHTS + OUTPUT_BIAS;
	// open the nn file
	FILE* nn = fopen(file, "rb");

	// if it's not invalid read the config values from it
	if (nn) {
		read += fread(inputWeights, sizeof(int16_t), INPUT_WEIGHTS * HIDDEN_WEIGHTS, nn);
		read += fread(hiddenBias, sizeof(int16_t), HIDDEN_BIAS, nn);
		read += fread(hiddenWeights, sizeof(int16_t), HIDDEN_WEIGHTS, nn);
		read += fread(outputBias, sizeof(int32_t), OUTPUT_BIAS, nn);

		if (read != fileSize)
		{
			std::cout << "Error loading the net, aborting ";
			exit(1);
		}

		// after reading the config we can close the file
		fclose(nn);
	}
	else {
		//if we don't find the nnue file we use the net embedded in the exe
		uint64_t memoryIndex = 0;
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

void NNUE::add(NNUE::accumulator& board_accumulator, int piece, int to)
{
	int inputNum = GetIndex(piece, to);
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		board_accumulator[i] += inputWeights[inputNum * HIDDEN_BIAS + i];
	}
}

void NNUE::clear(NNUE::accumulator& board_accumulator, int piece, int from)
{
	int inputNum = GetIndex(piece, from);
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		board_accumulator[i] -= inputWeights[inputNum * HIDDEN_BIAS + i];
	}
}

void NNUE::move(NNUE::accumulator& board_accumulator, int piece, int from, int to)
{
	int inputNumFrom = GetIndex(piece, from);
	int inputNumTo = GetIndex(piece, to);
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		board_accumulator[i] = board_accumulator[i] - inputWeights[inputNumFrom * HIDDEN_BIAS + i] + inputWeights[inputNumTo * HIDDEN_BIAS + i];
	}
}

int32_t NNUE::output(const NNUE::accumulator& board_accumulator) {
	//this function takes the net output for the current accumulators and returns the eval of the position according to the net
	int32_t output = outputBias[0];
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		output += relu(board_accumulator[i]) * hiddenWeights[i];
	}
	return output / (64 * 256);
}

void NNUE::Clear(NNUE::accumulator& board_accumulator) {
	//Reset the accumulators of the nnue
	for (int i = 0; i < HIDDEN_BIAS; i++) {
		board_accumulator[i] = 0;
	}
}

int NNUE::GetIndex(int piece, int square)
{
	int piecetype = GetPieceType(piece);
	return square + piecetype * 64 + (Color[piece] == BLACK) * 64 * 6;
}
