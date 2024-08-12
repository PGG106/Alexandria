#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "bitboard.h"
#include "simd.h"
#include "types.h"

#define NETUP true

// Net arch: (768 -> L1_SIZE) x 2 -> (L2_SIZE -> L3_SIZE -> 1) x OUTPUT_BUCKETS
constexpr int NUM_INPUTS = 768;
constexpr int L1_SIZE = 1536;
constexpr int L2_SIZE = 16;
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT  = 362;
constexpr int FT_SHIFT  = 10;
constexpr int L1_QUANT  = 45;
constexpr int NET_SCALE = 400;

constexpr float L1_MUL  = float(1 << FT_SHIFT) / float(FT_QUANT * FT_QUANT * L1_QUANT);
constexpr float WEIGHT_CLIPPING = 1.414f;
static_assert(std::round(L1_QUANT * WEIGHT_CLIPPING) * (FT_QUANT * FT_QUANT >> FT_SHIFT) * 4 <= 32767);

#if defined(USE_SIMD)
constexpr int FT_CHUNK_SIZE = sizeof(vepi16) / sizeof(int16_t);
constexpr int L1_CHUNK_SIZE = sizeof(vepi8 ) / sizeof(int8_t);
constexpr int L2_CHUNK_SIZE = sizeof(vps32 ) / sizeof(float);
constexpr int L3_CHUNK_SIZE = sizeof(vps32 ) / sizeof(float);
constexpr int L1_CHUNK_PER_32 = sizeof(int32_t) / sizeof(int8_t);
#else
constexpr int L1_CHUNK_PER_32 = 1;
#endif

using NNUEIndices = std::array<std::size_t, 2>;

struct UnquantisedNetwork {
    float FTWeights[NUM_INPUTS * L1_SIZE];
    float FTBiases [L1_SIZE];
    float L1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float L1Biases [OUTPUT_BUCKETS][L2_SIZE];
    float L2Weights[L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float L2Biases [OUTPUT_BUCKETS][L3_SIZE];
    float L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float L3Biases [OUTPUT_BUCKETS];
};

struct QuantisedNetwork {
    int16_t FTWeights[NUM_INPUTS * L1_SIZE];
    int16_t FTBiases [L1_SIZE];
    int8_t  L1Weights[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
    float   L1Biases [OUTPUT_BUCKETS][L2_SIZE];
    float   L2Weights[L2_SIZE][OUTPUT_BUCKETS][L3_SIZE];
    float   L2Biases [OUTPUT_BUCKETS][L3_SIZE];
    float   L3Weights[L3_SIZE][OUTPUT_BUCKETS];
    float   L3Biases [OUTPUT_BUCKETS];
};

struct Network {
    alignas(64) int16_t FTWeights[NUM_INPUTS * L1_SIZE];
    alignas(64) int16_t FTBiases [L1_SIZE];
    alignas(64) int8_t  L1Weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    alignas(64) float   L1Biases [OUTPUT_BUCKETS][L2_SIZE];
    alignas(64) float   L2Weights[OUTPUT_BUCKETS][L2_SIZE * L3_SIZE];
    alignas(64) float   L2Biases [OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float   L3Weights[OUTPUT_BUCKETS][L3_SIZE];
    alignas(64) float   L3Biases [OUTPUT_BUCKETS];
};

struct NNZData {
    uint64_t data[L1_SIZE / 2]{};
    int indexOrder[L1_SIZE / 2];

    NNZData() {
        for (int i = 0; i < L1_SIZE / 2; ++i) indexOrder[i] = i;
    };

    inline void update(auto *outputs) {
        for (int i = 0; i < L1_SIZE; ++i) data[i % (L1_SIZE / 2)] += bool(outputs[i]);
    };

    inline void permute(QuantisedNetwork &quantisedNet) {
        std::stable_sort(indexOrder, indexOrder + L1_SIZE / 2, [&](const int &a, const int &b) { return data[a] < data[b]; });

        std::memcpy(FTWeightsCopy, quantisedNet.FTWeights, sizeof(quantisedNet.FTWeights));
        std::memcpy(FTBiasesCopy , quantisedNet.FTBiases , sizeof(quantisedNet.FTBiases ));
        std::memcpy(L1WeightsCopy, quantisedNet.L1Weights, sizeof(quantisedNet.L1Weights));

        for (int i = 0; i < L1_SIZE / 2; ++i) {
            int oldIndex1 = indexOrder[i];
            int newIndex1 = i;
            int oldIndex2 = oldIndex1 + L1_SIZE / 2;
            int newIndex2 = newIndex1 + L1_SIZE / 2;

            for (int j = 0; j < NUM_INPUTS; ++j) {
                quantisedNet.FTWeights[j * L1_SIZE + newIndex1] = FTWeightsCopy[j * L1_SIZE + oldIndex1];
                quantisedNet.FTWeights[j * L1_SIZE + newIndex2] = FTWeightsCopy[j * L1_SIZE + oldIndex2];
            }

            quantisedNet.FTBiases[newIndex1] = FTWeightsCopy[oldIndex1];
            quantisedNet.FTBiases[newIndex2] = FTWeightsCopy[oldIndex2];

            for (int j = 0; j < OUTPUT_BUCKETS; ++j) {
                for (int k = 0; k < L2_SIZE; ++k) {
                    quantisedNet.L1Weights[newIndex1][j][k] = L1WeightsCopy[oldIndex1][j][k];
                    quantisedNet.L1Weights[newIndex2][j][k] = L1WeightsCopy[oldIndex2][j][k];
                }
            }
        }
    };

    private:
    int16_t FTWeightsCopy[NUM_INPUTS * L1_SIZE];
    int16_t FTBiasesCopy [L1_SIZE];
    int8_t  L1WeightsCopy[L1_SIZE][OUTPUT_BUCKETS][L2_SIZE];
};

struct NNZEntry {
    uint16_t indices[8];
    int      count;
};

struct NNZTable {
    NNZEntry table[256];

    NNZTable() {
        for (uint16_t i = 0; i <= 255; ++i) {
            this->table[i].count = CountBits(i);
            Bitboard j = i;
            uint16_t k = 0;
            while (j)
                this->table[i].indices[k++] = popLsb(j);
        }
    };
};

struct Position;

class NNUE {
public:
    // per pov accumulator
    struct Pov_Accumulator {
        alignas(64) std::array<int16_t, L1_SIZE> values;
        int pov;
        std::vector<std::size_t> NNUEAdd = {};
        std::vector<std::size_t> NNUESub = {};
        bool needsRefresh = false;

        void accumulate(Position *pos);
        [[nodiscard]] int GetIndex(const int piece, const int square, const bool flip) const;
        void addSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub);
        void addSubSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub1, std::size_t sub2);
        void applyUpdate(Pov_Accumulator& previousPovAccumulator);

        [[nodiscard]] bool isClean() const {
            return NNUEAdd.empty();
        }
    };
// final total accumulator that holds the 2 povs
    struct Accumulator {

        Accumulator(){
            this->perspective[WHITE].pov = WHITE;
            this->perspective[BLACK].pov = BLACK;
        }

        std::array<Pov_Accumulator, 2> perspective;

        void AppendAddIndex(int piece, int square, std::array<bool, 2> flip) {
            assert(this->perspective[WHITE].NNUEAdd.size() <= 1);
            assert(this->perspective[BLACK].NNUEAdd.size() <= 1);
            this->perspective[WHITE].NNUEAdd.emplace_back(perspective[WHITE].GetIndex(piece,square,flip[WHITE]));
            this->perspective[BLACK].NNUEAdd.emplace_back(perspective[BLACK].GetIndex(piece,square,flip[BLACK]));
        }

        void AppendSubIndex(int piece, int square, std::array<bool, 2> flip) {
            assert(this->perspective[WHITE].NNUESub.size() <= 1);
            assert(this->perspective[BLACK].NNUESub.size() <= 1);
            this->perspective[WHITE].NNUESub.emplace_back(perspective[WHITE].GetIndex(piece,square,flip[WHITE]));
            this->perspective[BLACK].NNUESub.emplace_back(perspective[BLACK].GetIndex(piece,square,flip[BLACK]));
        }

        void ClearAddIndex() {
            this->perspective[WHITE].NNUEAdd.clear();
            this->perspective[BLACK].NNUEAdd.clear();
        }

        void ClearSubIndex() {
            this->perspective[WHITE].NNUESub.clear();
            this->perspective[BLACK].NNUESub.clear();
        }
    };

    static void init(const char *file);
    static void finish_netup([[maybe_unused]] const char *file);
    static void accumulate(NNUE::Accumulator &board_accumulator, Position* pos);
    static void update(Accumulator *acc, Position* pos);
    static void ActivateFT(const int16_t *us, const int16_t *them, uint16_t *nnzIndices, int &nnzCount, uint8_t *output);
    static void PropagateL1(const uint8_t *inputs, uint16_t *nnzIndices, int nnzCount, const int8_t *weights, const float *biases, float *output);
    static void PropagateL2(const float *inputs, const float *weights, const float *biases, float *output);
    static void PropagateL3(const float *inputs, const float *weights, const float bias, float &output);
    [[nodiscard]] static int32_t output(const NNUE::Accumulator &board_accumulator, const int stm, const int outputBucket);
};