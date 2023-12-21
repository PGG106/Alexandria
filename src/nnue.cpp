#include "nnue.h"
#include <algorithm>
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
const unsigned char gEVALData[1] = {};
const unsigned char* const gEVALEnd = &gEVALData[1];
const unsigned int gEVALSize = 1;
#endif

Network net;

// Thanks to Disservin for having me look at his code and Luecx for the
// invaluable help and the immense patience

int32_t NNUE::SCReLU(int16_t x) {
    constexpr int16_t CR_MIN = 0;
    constexpr int16_t CR_MAX = 255;
    // compute squared clipped ReLU
    int16_t clipped = std::clamp(x, CR_MIN, CR_MAX);
    int32_t wide = clipped;
    return wide * wide;
}

void NNUE::init(const char* file) {
    // open the nn file
    FILE* nn = fopen(file, "rb");

    // if it's not invalid read the config values from it
    if (nn) {
        // initialize an accumulator for every input of the second layer
        size_t read = 0;
        size_t fileSize = sizeof(Network);
        size_t objectsExpected = fileSize / sizeof(int16_t);

        read += fread(net.featureWeights, sizeof(int16_t), INPUT_WEIGHTS * HIDDEN_SIZE, nn);
        read += fread(net.featureBias, sizeof(int16_t), HIDDEN_SIZE, nn);
        read += fread(net.outputWeights, sizeof(int16_t), HIDDEN_SIZE * 2, nn);
        read += fread(&net.outputBias, sizeof(int16_t), 1, nn);

        if (read != objectsExpected) {
            std::cout << "Error loading the net, aborting ";
            std::cout << "Expected " << objectsExpected << " shorts, got " << read << "\n";
            exit(1);
        }

        // after reading the config we can close the file
        fclose(nn);
    } else {
        // if we don't find the nnue file we use the net embedded in the exe
        uint64_t memoryIndex = 0;
        std::memcpy(net.featureWeights, &gEVALData[memoryIndex], INPUT_WEIGHTS * HIDDEN_SIZE * sizeof(int16_t));
        memoryIndex += INPUT_WEIGHTS * HIDDEN_SIZE * sizeof(int16_t);
        std::memcpy(net.featureBias, &gEVALData[memoryIndex], HIDDEN_SIZE * sizeof(int16_t));
        memoryIndex += HIDDEN_SIZE * sizeof(int16_t);

        std::memcpy(net.outputWeights, &gEVALData[memoryIndex], HIDDEN_SIZE * sizeof(int16_t) * 2);
        memoryIndex += HIDDEN_SIZE * sizeof(int16_t) * 2;
        std::memcpy(&net.outputBias, &gEVALData[memoryIndex], 1 * sizeof(int16_t));
    }
}

void NNUE::add(NNUE::accumulator& board_accumulator, const int piece, const int to) {
    auto [whiteIdx, blackIdx] = GetIndex(piece, to);
    auto whiteAdd = &net.featureWeights[whiteIdx * HIDDEN_SIZE];
    auto blackAdd = &net.featureWeights[blackIdx * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[0][i] += whiteAdd[i];
    }
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[1][i] += blackAdd[i];
    }
}

void NNUE::clear(NNUE::accumulator& board_accumulator, const int piece, const int from) {
    auto [whiteIdx, blackIdx] = GetIndex(piece, from);
    auto whiteSub = &net.featureWeights[whiteIdx * HIDDEN_SIZE];
    auto blackSub = &net.featureWeights[blackIdx * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[0][i] -= whiteSub[i];
    }
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[1][i] -= blackSub[i];
    }
}

void NNUE::move(NNUE::accumulator& board_accumulator, const int piece, const int from, const int to) {
    auto [whiteIdxFrom, blackIdxFrom] = GetIndex(piece, from);
    auto [whiteIdxTo, blackIdxTo] = GetIndex(piece, to);
    auto whiteSub = &net.featureWeights[whiteIdxFrom * HIDDEN_SIZE];
    auto whiteAdd = &net.featureWeights[whiteIdxTo * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[0][i] = board_accumulator[0][i] - whiteSub[i] + whiteAdd[i];
    }
    auto blackSub = &net.featureWeights[blackIdxFrom * HIDDEN_SIZE];
    auto blackAdd = &net.featureWeights[blackIdxTo * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[1][i] = board_accumulator[1][i] - blackSub[i] + blackAdd[i];
    }
}

int32_t NNUE::output(const NNUE::accumulator& board_accumulator, const bool whiteToMove) {
    // this function takes the net output for the current accumulators and returns the eval of the position
    // according to the net
    const int16_t* us;
    const int16_t* them;
    if (whiteToMove) {
        us = board_accumulator[0].data();
        them = board_accumulator[1].data();
    } else {
        us = board_accumulator[1].data();
        them = board_accumulator[0].data();
    }
    int32_t output = 0;
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        output += SCReLU(us[i]) * static_cast<int32_t>(net.outputWeights[i]);
    }
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        output += SCReLU(them[i]) * static_cast<int32_t>(net.outputWeights[HIDDEN_SIZE + i]);
    }
    int32_t unsquared = output / 255 + net.outputBias;
    return unsquared * 400 / (64 * 255);
}

std::pair<std::size_t, std::size_t> NNUE::GetIndex(const int piece, const int square) {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    int piecetype = GetPieceType(piece);
    int color = piece / 6;
    // return square + piecetype * 64 + (Color[piece] == BLACK) * 64 * 6;
    std::size_t whiteIdx = color * COLOR_STRIDE + piecetype * PIECE_STRIDE + (square ^ 0b111'000);
    std::size_t blackIdx = (1 ^ color) * COLOR_STRIDE + piecetype * PIECE_STRIDE + square;
    return {whiteIdx, blackIdx};
}
