#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdint>
#include <cstring>
#ifdef NNZ_PROFILE
#include <array>
#include <cstdlib>
#include <iostream>
#include <mutex>
#endif
#include "incbin/incbin.h"
#include <fstream>
#include "io.h"

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
const unsigned char *const gEVALEnd = &gEVALData[1];
const unsigned int gEVALSize = 1;
#endif

const Network *net;
NNZTable nnzTable;

#ifdef NNZ_PROFILE
namespace {
#ifndef NNZ_PROFILE_SAMPLES
#define NNZ_PROFILE_SAMPLES 100000
#endif

constexpr uint64_t NnzProfileSamples = NNZ_PROFILE_SAMPLES;
constexpr std::size_t NnzProfileMaskBytes = (L1_SIZE / 2) / 8;
static_assert((L1_SIZE / 2) % 8 == 0);

const char* nnzProfilePath() {
    const char* path = std::getenv("NNZ_PROFILE_OUTPUT");
    return path != nullptr ? path : "nnz-masks.bin";
}

std::ofstream nnzProfile{nnzProfilePath(), std::ios::binary};
std::mutex nnzProfileMutex;
uint64_t nnzProfileSamples = 0;

void recordNnzProfile(const uint8_t* output) {
    std::lock_guard lock{nnzProfileMutex};
    if (nnzProfileSamples >= NnzProfileSamples)
        return;
    if (!nnzProfile) {
        std::cerr << "Error: Could not write NNZ profile to " << nnzProfilePath() << '\n';
        std::abort();
    }

    std::array<uint8_t, NnzProfileMaskBytes> mask = {};
    for (int lane = 0; lane < L1_SIZE / 2; ++lane)
        mask[lane / 8] |= static_cast<uint8_t>((output[lane] != 0) << (lane % 8));

    nnzProfile.write(reinterpret_cast<const char*>(mask.data()), mask.size());
    ++nnzProfileSamples;
}
}
#endif

UnquantisedNetwork unquantisedNet;
QuantisedNetwork quantisedNet;
Network permutedNet;

void load_unquantize_andquant() {
    // open the nn file
    std::ifstream stream{"raw.bin", std::ios::binary};

    stream.read(reinterpret_cast<char *>(&unquantisedNet), sizeof(UnquantisedNetwork));

    // Merge factoriser  + quantise FT weights
    for (int bucket = 0; bucket < INPUT_BUCKETS; ++bucket) {
        int bucket_offset = bucket * (NUM_INPUTS * L1_SIZE);

        for (int i = 0; i < NUM_INPUTS * L1_SIZE; ++i) {
            float w = unquantisedNet.FTWeights[bucket_offset + i] + unquantisedNet.Factoriser[i];

            quantisedNet.FTWeights[bucket_offset + i] = static_cast<int16_t>(std::round(w * FT_QUANT));
        }
    }

    // Quantise FT Biases
    for (int i = 0; i < L1_SIZE; ++i)
        quantisedNet.FTBiases[i] = static_cast<int16_t>(std::round(unquantisedNet.FTBiases[i] * FT_QUANT));

    // Quantise L1, L2 and L3 weights and biases
    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket) {
        // Quantise L1 Weights
        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                quantisedNet.L1Weights[i][bucket][j] = static_cast<int8_t>(std::round(
                    unquantisedNet.L1Weights[i][bucket][j] * L1_QUANT));

        // Quantise L1 Biases
        for (int i = 0; i < L2_SIZE; ++i) {
            quantisedNet.L1Biases[bucket][i] = unquantisedNet.L1Biases[bucket][i];
        }

        // Quantise L2 Weights
        for (int i = 0; i < EFFECTIVE_L2_SIZE; ++i)
            for (int j = 0; j < L3_SIZE; ++j)
                quantisedNet.L2Weights[i][bucket][j] = unquantisedNet.L2Weights[i][bucket][j];

        // Quantise L2 Biases
        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L2Biases[bucket][i] = unquantisedNet.L2Biases[bucket][i];

        // Quantise L3 Weights
        for (int i = 0; i < L3_SIZE; ++i)
            quantisedNet.L3Weights[i][bucket] = unquantisedNet.L3Weights[i][bucket];

        // Quantise L3 Biases
        quantisedNet.L3Biases[bucket] = unquantisedNet.L3Biases[bucket];
    }

    std::ofstream out{"nn.net", std::ios::binary};
    out.write(reinterpret_cast<const char *>(&quantisedNet), sizeof(QuantisedNetwork));
    exit(12);
}

void NNUE::init() {
    net = reinterpret_cast<const Network *>(gEVALData);
}

void NNUE::AccumulatorStack::reset(Position* pos) {
    head = 0;
    Accumulator& root = entries[0];
    root.updated = {false, false};
    root.kings = {static_cast<Square>(KingSQ(pos, WHITE)), static_cast<Square>(KingSQ(pos, BLACK))};
}

using KingView = uint8_t;

static KingView GetKingView(const Square kingSquare, const int side) {
    return static_cast<KingView>(getBucket(kingSquare, side) << 1 | (get_file[kingSquare] > 3));
}

template <uint8_t RemovedCount, uint8_t AddedCount>
static void ApplyDirtyPieces(const DirtyPieces& dirtyPieces, const int side, const KingView kingView,
                             const NNUE::PovAccumulator& input, NNUE::PovAccumulator& output) {
    const int kingBucket = kingView >> 1;
    const bool flip = kingView & 1;
    const int16_t* removedWeights[RemovedCount];
    const int16_t* addedWeights[AddedCount];

    for (uint8_t index = 0; index < RemovedCount; ++index) {
        const SquarePiece& removed = dirtyPieces.removed[index];
        const size_t feature = NNUE::getIndex(removed.piece, removed.square, side, kingBucket, flip);
        removedWeights[index] = &net->FTWeights[feature];
    }
    for (uint8_t index = 0; index < AddedCount; ++index) {
        const SquarePiece& added = dirtyPieces.added[index];
        const size_t feature = NNUE::getIndex(added.piece, added.square, side, kingBucket, flip);
        addedWeights[index] = &net->FTWeights[feature];
    }

#if defined(USE_SIMD)
    for (int lane = 0; lane < L1_SIZE; lane += FT_CHUNK_SIZE) {
        vepi16 result = vec_load_epi(reinterpret_cast<const vepi16*>(&input[lane]));
        for (uint8_t index = 0; index < RemovedCount; ++index)
            result = vec_sub_epi16(result, vec_load_epi(reinterpret_cast<const vepi16*>(&removedWeights[index][lane])));
        for (uint8_t index = 0; index < AddedCount; ++index)
            result = vec_add_epi16(result, vec_load_epi(reinterpret_cast<const vepi16*>(&addedWeights[index][lane])));
        vec_store_epi(reinterpret_cast<vepi16*>(&output[lane]), result);
    }
#else
    for (int lane = 0; lane < L1_SIZE; ++lane) {
        int16_t result = input[lane];
        for (uint8_t index = 0; index < RemovedCount; ++index)
            result -= removedWeights[index][lane];
        for (uint8_t index = 0; index < AddedCount; ++index)
            result += addedWeights[index][lane];
        output[lane] = result;
    }
#endif
}

static void ApplyDirtyPieces(const DirtyPieces& dirtyPieces, const int side, const KingView kingView,
                             const NNUE::PovAccumulator& input, NNUE::PovAccumulator& output) {
    if (dirtyPieces.removedCount == 1 && dirtyPieces.addedCount == 1)
        ApplyDirtyPieces<1, 1>(dirtyPieces, side, kingView, input, output);
    else if (dirtyPieces.removedCount == 2 && dirtyPieces.addedCount == 1)
        ApplyDirtyPieces<2, 1>(dirtyPieces, side, kingView, input, output);
    else {
        assert(dirtyPieces.removedCount == 2 && dirtyPieces.addedCount == 2);
        ApplyDirtyPieces<2, 2>(dirtyPieces, side, kingView, input, output);
    }
}

static void RefreshAccumulator(Position* pos, NNUE::FinnyTable* finnyTable, NNUE::Accumulator& accumulator,
                               const int side, const KingView kingView) {
    const int kingBucket = kingView >> 1;
    const bool flip = kingView & 1;
    NNUE::FinnyTableEntry& cachedEntry = (*finnyTable)[side][kingBucket][flip];

    size_t add[32], remove[32];
    size_t addCount = 0, removeCount = 0;

    for (int piece = WP; piece <= BK; ++piece) {
        Bitboard added = pos->state().bitboards[piece] & ~cachedEntry.occupancies[piece];
        Bitboard removed = cachedEntry.occupancies[piece] & ~pos->state().bitboards[piece];
        while (added) {
            const int square = popLsb(added);
            add[addCount++] = NNUE::getIndex(piece, square, side, kingBucket, flip);
        }
        while (removed) {
            const int square = popLsb(removed);
            remove[removeCount++] = NNUE::getIndex(piece, square, side, kingBucket, flip);
        }
        cachedEntry.occupancies[piece] = pos->state().bitboards[piece];
    }

    const size_t pairedCount = std::min(addCount, removeCount);
    for (size_t index = 0; index < pairedCount; ++index) {
        for (int lane = 0; lane < L1_SIZE; ++lane)
            cachedEntry.accumCache[lane] += net->FTWeights[add[index] + lane]
                                          - net->FTWeights[remove[index] + lane];
    }
    for (size_t index = pairedCount; index < addCount; ++index) {
        for (int lane = 0; lane < L1_SIZE; ++lane)
            cachedEntry.accumCache[lane] += net->FTWeights[add[index] + lane];
    }
    for (size_t index = pairedCount; index < removeCount; ++index) {
        for (int lane = 0; lane < L1_SIZE; ++lane)
            cachedEntry.accumCache[lane] -= net->FTWeights[remove[index] + lane];
    }

    accumulator.colors[side] = cachedEntry.accumCache;
    accumulator.updated[side] = true;
}

static void ResolveAccumulator(Position* pos, NNUE::FinnyTable* finnyTable,
                               NNUE::AccumulatorStack* accumulatorStack, const int side) {
    NNUE::Accumulator* const root = accumulatorStack->entries.data();
    NNUE::Accumulator* const target = &accumulatorStack->current();
    if (target->updated[side])
        return;

    const KingView kingView = GetKingView(target->kings[side], side);
    NNUE::Accumulator* source = target;
    while (source > root) {
        NNUE::Accumulator* const parent = source - 1;
        if (GetKingView(parent->kings[side], side) != kingView)
            break;
        source = parent;
        if (source->updated[side]) {
            while (source < target) {
                NNUE::Accumulator* const child = source + 1;
                ApplyDirtyPieces(child->dirtyPieces, side, kingView,
                                 source->colors[side], child->colors[side]);
                child->updated[side] = true;
                source = child;
            }
            return;
        }
    }

    RefreshAccumulator(pos, finnyTable, *target, side, kingView);
}

// does FT activate for one pov at a time
void NNUE::povActivateAffine(Position *pos, NNUE::FinnyTable *FinnyPointer,
                             NNUE::AccumulatorStack* accumulatorStack, const int side, uint16_t *base,
                             uint16_t *nnzIndices, int &nnzCount, uint8_t *output) {
    ResolveAccumulator(pos, FinnyPointer, accumulatorStack, side);
    NNUE::PovAccumulator &accumCache = accumulatorStack->current().colors[side];

    for (int i = 0; i < L1_SIZE / 2; ++i) {
        int16_t clipped0 = std::clamp<int16_t>(accumCache[i], 0, FT_QUANT);
        int16_t clipped1 = std::clamp<int16_t>(accumCache[i + L1_SIZE / 2], 0, FT_QUANT);
        output[i] = static_cast<uint8_t>(clipped0 * clipped1 >> FT_SHIFT);
    }
#endif

#ifdef NNZ_PROFILE
    recordNnzProfile(output);
#endif
}

void NNUE::propagateL1(const uint8_t *inputs, [[maybe_unused]] uint16_t *nnzIndices, [[maybe_unused]] int nnzCount, const int8_t *weights, const float *biases, float *output) {
#if defined(USE_SIMD)
    vepi32 sums[L2_SIZE / L2_CHUNK_SIZE] = {};
    const int32_t *inputs32 = reinterpret_cast<const int32_t *>(inputs);

    // We read in the inputs in chunks of 4 (as dpbusd horizontally sums by 4).
    // Then, each chunk of 4 is multiplied by the L1 weights. (The weights are pre-permuted to allow us to do this)
    // We also unroll by 2 to save a madd every 2 multiplications (in the non VNNI case).
    // Note that we sacrificed some quantisation accuracy to do this, as the additional accuracy had no elo gain.
    int i = 0;
    for (; i < nnzCount - 1; i += 2) {
        const uint16_t indexa = nnzIndices[i + 0];
        const uint16_t indexb = nnzIndices[i + 1];
        const vepi32 input32a = vec_set1_epi32(inputs32[indexa]);
        const vepi32 input32b = vec_set1_epi32(inputs32[indexb]);
        const vepi8 *weighta  = reinterpret_cast<const vepi8*>(&weights[indexa * L1_CHUNK_PER_32 * L2_SIZE]);
        const vepi8 *weightb  = reinterpret_cast<const vepi8*>(&weights[indexb * L1_CHUNK_PER_32 * L2_SIZE]);
        for (int j = 0; j < L2_SIZE / L2_CHUNK_SIZE; ++j)
            sums[j] = vec_dpbusdx2_epi32(sums[j], input32a, weighta[j], input32b, weightb[j]);
    }

    for (; i < nnzCount; ++i) {
        const uint16_t index = nnzIndices[i];
        const vepi32 input32 = vec_set1_epi32(inputs32[index]);
        const vepi8 *weight  = reinterpret_cast<const vepi8*>(&weights[index * L1_CHUNK_PER_32 * L2_SIZE]);
        for (int j = 0; j < L2_SIZE / L2_CHUNK_SIZE; ++j)
            sums[j] = vec_dpbusd_epi32(sums[j], input32, weight[j]);
    }

    // We divide by the ONE value to proceed into the later layers, which is carried out in floats.
    // A nice trick by ciekce: instead of dividing, and then adding the L1 bias, we multiply by its reciprocal,
    // and then add the bias, which allows us to use FMA.
    for (i = 0; i < L2_SIZE / L2_CHUNK_SIZE; ++i) {
        // Convert into floats, and activate L1
        const vps32 biasVec = vec_load_ps(&biases[i * L2_CHUNK_SIZE]);
        const vps32 sumMul = vec_set1_ps(L1_MUL);
        const vps32 sumPs = vec_mul_add_ps(vec_cvtepi32_ps(sums[i]), sumMul, biasVec);

        const vps32 Zero = vec_zero_ps();
        const vps32 One = vec_set1_ps(1.0f);
        // linear
        const vps32 clipped = vec_min_ps(vec_max_ps(sumPs, Zero), One);
        // squared
        const vps32 squared = vec_mul_ps(sumPs, sumPs);
        const vps32 squared_clipped = vec_min_ps(vec_max_ps(squared, Zero), One);
        // it's storing time
        vec_store_ps(&output[i * L2_CHUNK_SIZE], clipped);
        vec_store_ps(&output[L2_SIZE + i * L2_CHUNK_SIZE], squared_clipped);
    }
#else
    int sums[L2_SIZE] = {};
    for (int i = 0; i < L1_SIZE; ++i) {
        for (int j = 0; j < L2_SIZE; ++j) {
            sums[j] += static_cast<int32_t>(inputs[i] * weights[j * L1_SIZE + i]);
        }
    }

    for (int i = 0; i < L2_SIZE; ++i) {
        // Convert into floats and activate L1
        const float z = float(sums[i]) * L1_MUL + biases[i];
        // Dual activation: produce 2 L1 outputs for each input by applying different activations
        const float squared = std::clamp(z * z, 0.0f, 1.0f);
        const float linear =  std::clamp(z, 0.0f, 1.0f);
        output[i] = linear;
        output[i+ L2_SIZE] = squared;
    }
#endif
}

void NNUE::propagateL2(const float *inputs, const float *weights, const float *biases, float *output) {
    // For each input, multiply by all the L2 weights
#if defined(USE_SIMD)
    vps32 sumVecs[L3_SIZE / L3_CHUNK_SIZE];

    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i)
        sumVecs[i] = vec_load_ps(&biases[i * L3_CHUNK_SIZE]);

    for (int i = 0; i < EFFECTIVE_L2_SIZE; ++i) {
        const vps32 inputVec = vec_set1_ps(inputs[i]);
        const vps32 *weight = reinterpret_cast<const vps32 *>(&weights[i * L3_SIZE]);
        for (int j = 0; j < L3_SIZE / L3_CHUNK_SIZE; ++j)
            sumVecs[j] = vec_mul_add_ps(inputVec, weight[j], sumVecs[j]);
    }

    // Activate L2
    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i) {
        const vps32 Zero = vec_zero_ps();
        const vps32 One = vec_set1_ps(1.0f);
        const vps32 clipped = vec_min_ps(vec_max_ps(sumVecs[i], Zero), One);
        const vps32 squared = vec_mul_ps(clipped, clipped);
        vec_store_ps(&output[i * L3_CHUNK_SIZE], squared);
    }
#else
    float sums[L3_SIZE];

    for (int i = 0; i < L3_SIZE; ++i)
        sums[i] = biases[i];

    // Affine transform for L2
    for (int i = 0; i < EFFECTIVE_L2_SIZE; ++i) {
        const float *weight = &weights[i * L3_SIZE];
        for (int out = 0; out < L3_SIZE; ++out) {
            sums[out] += inputs[i] * weight[out];
        }
    }

    // Activate L2
    for (int i = 0; i < L3_SIZE; ++i) {
        const float clipped = std::clamp(sums[i], 0.0f, 1.0f);
        const float squared = clipped * clipped;
        output[i] = squared;
    }
#endif
}

void NNUE::propagateL3(const float *inputs, const float *weights, const float bias, float &output) {
    constexpr int avx512chunk = 512 / 32;
#if defined(USE_SIMD)
    constexpr int numSums = avx512chunk / (sizeof(vps32) / sizeof(float));
    vps32 sumVecs[numSums] = {};
    // Affine transform for L3
    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i) {
        const vps32 weightVec = vec_load_ps(&weights[i * L3_CHUNK_SIZE]);
        const vps32 inputsVec = vec_load_ps(&inputs[i * L3_CHUNK_SIZE]);
        sumVecs[i % numSums] = vec_mul_add_ps(inputsVec, weightVec, sumVecs[i % numSums]);
    }
    output = vec_reduce_add_ps(sumVecs) + bias;
#else
    constexpr int numSums = avx512chunk;
    float sums[numSums] = {};

    // Affine transform for L3
    for (int i = 0; i < L3_SIZE; ++i) {
        sums[i % numSums] += inputs[i] * weights[i];
    }
    output = reduce_add(sums, numSums) + bias;
#endif
}

void NNUE::activateAffine(Position *pos, NNUE::FinnyTable *FinnyPointer, NNUE::AccumulatorStack* accumulatorStack,
                          [[maybe_unused]] uint16_t *base, [[maybe_unused]] uint16_t *nnzIndices,
                          [[maybe_unused]] int &nnzCount, uint8_t *output) {
    povActivateAffine(pos, FinnyPointer, accumulatorStack, pos->side, base, nnzIndices, nnzCount, output);
    povActivateAffine(pos, FinnyPointer, accumulatorStack, pos->side ^ 1, base, nnzIndices, nnzCount,
                      &output[L1_SIZE / 2]);
}

int NNUE::output(Position *pos, NNUE::FinnyTable *FinnyPointer, NNUE::AccumulatorStack* accumulatorStack) {
    int nnzCount = 0;
    uint16_t base[8] = {}; // replaces v128i base
    alignas (64) uint16_t nnzIndices[L1_SIZE / L1_CHUNK_PER_32];

    const int pieceCount = pos->PieceCount();
    const int outputBucket = std::min((63 - pieceCount) * (32 - pieceCount) / 225, 7);
    alignas (64) uint8_t FTOutputs[L1_SIZE];
    alignas (64) float L1Outputs[EFFECTIVE_L2_SIZE];
    alignas (64) float L2Outputs[L3_SIZE];
    float L3Output;

    // does FT activation for both accumulators
    activateAffine(pos, FinnyPointer, accumulatorStack, base, nnzIndices, nnzCount, FTOutputs);

    propagateL1(FTOutputs, nnzIndices, nnzCount, net->L1Weights[outputBucket], net->L1Biases[outputBucket], L1Outputs);

    propagateL2(L1Outputs, net->L2Weights[outputBucket], net->L2Biases[outputBucket], L2Outputs);

    propagateL3(L2Outputs, net->L3Weights[outputBucket], net->L3Biases[outputBucket], L3Output);

    return L3Output * NET_SCALE;
}

size_t NNUE::getIndex(const int piece, const int square, const int side, const int bucket, const bool flip) {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece] * (!MERGE_KING_PLANES || piecetype == KING);
    const int pieceColorPov = pieceColor ^ side;

    // Get the final indexes of the updates, accounting for hm
    auto squarePov = square ^ (0b111'000 * !side) ^ (0b000'111 * flip);
    auto idx = bucket * NUM_INPUTS + pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return idx * L1_SIZE;
}
