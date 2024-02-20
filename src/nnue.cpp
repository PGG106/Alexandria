#include "nnue.h"
#include <algorithm>
#include "board.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <immintrin.h>
#include "incbin/incbin.h"

// Macro to embed the default efficiently updatable neural network (NNUE) file
// data in the engine binary (using incbin.h, by Dale Weiler).
// This macro invocation will declare the following three variables
//     const unsigned char        gEVALData[];  // a pointer to the embedded data
//     const unsigned char *const gEVALEnd;     // a marker to the end
//     const unsigned int         gEVALSize;    // the size of the embedded file
// Note that this does not work in Microsoft Visual Studio.
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

#if defined(USE_AVX512)
constexpr int32_t CHUNK_SIZE = 32;
#elif defined(USE_AVX2)
constexpr int32_t CHUNK_SIZE = 16;
#else
constexpr int32_t CHUNK_SIZE = 1;
#endif
constexpr int32_t REQUIRED_ITERS = HIDDEN_SIZE / CHUNK_SIZE;

#if defined(USE_AVX2)
int32_t NNUE::horizontal_add(const __m256i sum) {
    auto upper_128 = _mm256_extracti128_si256(sum, 1);
    auto lower_128 = _mm256_castsi256_si128(sum);
    auto sum_128 = _mm_add_epi32(upper_128, lower_128);

    auto upper_64 = _mm_unpackhi_epi64(sum_128, sum_128);
    auto sum_64 = _mm_add_epi32(upper_64, sum_128);

    auto upper_32 = _mm_shuffle_epi32(sum_64, 1);
    auto sum_32 = _mm_add_epi32(upper_32, sum_64);

    return _mm_cvtsi128_si32(sum_32);
}

int32_t NNUE::flatten(const int16_t *acc, const int16_t *weights) {
    auto sum = _mm256_setzero_si256();
    for (int i = 0; i < REQUIRED_ITERS; i++) {
        auto us_vector = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i * CHUNK_SIZE));
        auto weights_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(weights + i * CHUNK_SIZE));
        auto min = _mm256_set1_epi16(0);
        auto max = _mm256_set1_epi16(181);
        auto clamped = _mm256_min_epi16(_mm256_max_epi16(us_vector, min), max);
        auto mul = _mm256_madd_epi16(_mm256_mullo_epi16(clamped, weights_vec), clamped);
        sum = _mm256_add_epi32(sum, mul);
    }
    return horizontal_add(sum);
}

#elif defined(USE_AVX512)

int32_t NNUE::flatten(const int16_t *acc, const int16_t *weights) {
    auto sum = _mm512_setzero_si512();
    for (int i = 0; i < REQUIRED_ITERS; i++) {
        auto us_vector = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(acc + i * CHUNK_SIZE));
        auto weights_vec = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(weights + i * CHUNK_SIZE));
        auto min = _mm512_set1_epi16(0);
        auto max = _mm512_set1_epi16(181);
        auto clamped = _mm512_min_epi16(_mm512_max_epi16(us_vector, min), max);
        auto mul = _mm512_madd_epi16(_mm512_mullo_epi16(clamped, weights_vec), clamped);
        sum = _mm512_add_epi32(sum, mul);
    }
    return _mm512_reduce_add_epi32(sum);
}

#endif

int32_t NNUE::SCReLU(int16_t x) {
    constexpr int16_t CR_MIN = 0;
    constexpr int16_t CR_MAX = 181;
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

void NNUE::update(NNUE::accumulator& board_accumulator, std::vector<std::pair<std::size_t, std::size_t>>& NNUEAdd, std::vector<std::pair<std::size_t, std::size_t>>& NNUESub) {
    int adds = NNUEAdd.size();
    int subs = NNUESub.size();

    if (adds == 0 && subs == 0)
        return;

    // Quiets
    if (adds == 1 && subs == 1) {
        auto [whiteAddIdx, blackAddIdx] = NNUEAdd[0];
        auto [whiteSubIdx, blackSubIdx] = NNUESub[0];
        addSub(board_accumulator, whiteAddIdx, blackAddIdx, whiteSubIdx, blackSubIdx);
    }
    // Captures
    else if (adds == 1 && subs == 2) {
        auto [whiteAddIdx, blackAddIdx] = NNUEAdd[0];
        auto [whiteSubIdx1, blackSubIdx1] = NNUESub[0];
        auto [whiteSubIdx2, blackSubIdx2] = NNUESub[1];
        addSubSub(board_accumulator, whiteAddIdx, blackAddIdx, whiteSubIdx1, blackSubIdx1, whiteSubIdx2, blackSubIdx2);
    }
    // Castling
    else {
        auto [whiteAddIdx1, blackAddIdx1] = NNUEAdd[0];
        auto [whiteAddIdx2, blackAddIdx2] = NNUEAdd[1];
        auto [whiteSubIdx1, blackSubIdx1] = NNUESub[0];
        auto [whiteSubIdx2, blackSubIdx2] = NNUESub[1];
        addSub(board_accumulator, whiteAddIdx1, blackAddIdx1, whiteSubIdx1, blackSubIdx1);
        addSub(board_accumulator, whiteAddIdx2, blackAddIdx2, whiteSubIdx2, blackSubIdx2);
    }
    // Reset the add and sub vectors
    NNUEAdd.clear();
    NNUESub.clear();
}

void NNUE::addSub(NNUE::accumulator& board_accumulator, std::size_t whiteAddIdx, std::size_t blackAddIdx, std::size_t whiteSubIdx, std::size_t blackSubIdx) {
    auto whiteAdd = &net.featureWeights[whiteAddIdx * HIDDEN_SIZE];
    auto whiteSub = &net.featureWeights[whiteSubIdx * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[0][i] = board_accumulator[0][i] - whiteSub[i] + whiteAdd[i];
    }
    auto blackAdd = &net.featureWeights[blackAddIdx * HIDDEN_SIZE];
    auto blackSub = &net.featureWeights[blackSubIdx * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[1][i] = board_accumulator[1][i] - blackSub[i] + blackAdd[i];
    }
}

void NNUE::addSubSub(NNUE::accumulator& board_accumulator, 
                     std::size_t whiteAddIdx,
                     std::size_t blackAddIdx,
                     std::size_t whiteSubIdx1,
                     std::size_t blackSubIdx1,
                     std::size_t whiteSubIdx2,
                     std::size_t blackSubIdx2) {

    auto whiteAdd = &net.featureWeights[whiteAddIdx * HIDDEN_SIZE];
    auto whiteSub1 = &net.featureWeights[whiteSubIdx1 * HIDDEN_SIZE];
    auto whiteSub2 = &net.featureWeights[whiteSubIdx2 * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[0][i] = board_accumulator[0][i] - whiteSub1[i] - whiteSub2[i] + whiteAdd[i];
    }
    auto blackAdd = &net.featureWeights[blackAddIdx * HIDDEN_SIZE];
    auto blackSub1 = &net.featureWeights[blackSubIdx1 * HIDDEN_SIZE];
    auto blackSub2 = &net.featureWeights[blackSubIdx2 * HIDDEN_SIZE];
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[1][i] = board_accumulator[1][i] - blackSub1[i] - blackSub2[i] + blackAdd[i];
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
    #if defined(USE_AVX512) || defined(USE_AVX2)
    int32_t output = flatten(us, net.outputWeights) + flatten(them, net.outputWeights + HIDDEN_SIZE);
    return (net.outputBias + output / 181) * 400 / (64 * 181);
    #else
    int32_t output = 0;
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        output += SCReLU(us[i]) * static_cast<int32_t>(net.outputWeights[i]);
    }
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        output += SCReLU(them[i]) * static_cast<int32_t>(net.outputWeights[HIDDEN_SIZE + i]);
    }
    int32_t unsquared = output / 181 + net.outputBias;
    return unsquared * 400 / (64 * 181);
    #endif
}

std::pair<std::size_t, std::size_t> NNUE::GetIndex(const int piece, const int square) {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    int piecetype = GetPieceType(piece);
    int color = Color[piece];
    std::size_t whiteIdx = color * COLOR_STRIDE + piecetype * PIECE_STRIDE + (square ^ 0b111'000);
    std::size_t blackIdx = (1 ^ color) * COLOR_STRIDE + piecetype * PIECE_STRIDE + square;
    return {whiteIdx, blackIdx};
}
