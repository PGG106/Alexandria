#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdint>
#include <cstring>
#include "incbin/incbin.h"
#include "io.h"
#include <cstdlib>
#include <iostream>

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

void NNUE::init() {
    if (gEVALSize != sizeof(Network)) {
        std::cerr << "Invalid NNUE size: expected " << sizeof(Network)
                  << " bytes, got " << gEVALSize << std::endl;
        std::abort();
    }

    auto *rawNet = new Network;
    std::memcpy(rawNet, gEVALData, sizeof(Network));

    auto *preparedNet = static_cast<Network *>(std::aligned_alloc(64, sizeof(Network)));
    if (!preparedNet)
        std::abort();
    std::memcpy(preparedNet, rawNet, sizeof(Network));

#if defined(USE_SIMD)
    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket)
        for (int i = 0; i < L1_SIZE; i += L1_CHUNK_PER_32)
            for (int j = 0; j < L2_SIZE; ++j)
                for (int k = 0; k < L1_CHUNK_PER_32; ++k)
                    preparedNet->L1WeightsAlt[bucket][i * L2_SIZE + j * L1_CHUNK_PER_32 + k]
                        = rawNet->L1Weights[bucket][i + k][j];

    __m128i *weights = reinterpret_cast<__m128i *>(preparedNet->FTWeights);
    __m128i *biases = reinterpret_cast<__m128i *>(preparedNet->FTBiases);
    constexpr int weightsPerBlock = sizeof(__m128i) / sizeof(int16_t);
#if defined(USE_AVX512)
    constexpr int numRegs = 8;
    constexpr int packusOrder[numRegs] = {0, 2, 4, 6, 1, 3, 5, 7};
#else
    constexpr int numRegs = 4;
    constexpr int packusOrder[numRegs] = {0, 2, 1, 3};
#endif
    __m128i regs[numRegs];

    for (int i = 0; i < INPUT_BUCKETS * NUM_INPUTS * L1_SIZE / weightsPerBlock; i += numRegs) {
        for (int j = 0; j < numRegs; ++j)
            regs[j] = weights[i + j];
        for (int j = 0; j < numRegs; ++j)
            weights[i + j] = regs[packusOrder[j]];
    }

    for (int i = 0; i < L1_SIZE / weightsPerBlock; i += numRegs) {
        for (int j = 0; j < numRegs; ++j)
            regs[j] = biases[i + j];
        for (int j = 0; j < numRegs; ++j)
            biases[i + j] = regs[packusOrder[j]];
    }
#endif

    delete rawNet;
    net = preparedNet;
}

namespace {

const int16_t* featureAddress(const Square kingSquare, const int side, const int piece, const Square square) {
    const bool flip = get_file[kingSquare] > 3;
    const int bucket = getBucket(kingSquare, side);
    const auto offset = NNUE::getIndex(piece, square, side, bucket, flip);
    return &net->FTWeights[0][0][0][0][0] + offset;
}

Square kingSquare(const BoardState& state, const int side) {
    return static_cast<Square>(GetLsbIndex(state.bitboards[GetPiece(KING, side)]));
}

}

void NNUE::Accumulator::addPiece(const Square kingSquare, const int side, const int piece, const Square square) {
    const int16_t* weights = featureAddress(kingSquare, side, piece, square);
#if defined(USE_SIMD)
    for (int i = 0; i < L1_SIZE; i += FT_CHUNK_SIZE) {
        const vepi16 value = vec_load_epi(reinterpret_cast<const vepi16*>(&colors[side][i]));
        const vepi16 weight = vec_load_epi(reinterpret_cast<const vepi16*>(&weights[i]));
        vec_store_epi(reinterpret_cast<vepi16*>(&colors[side][i]), vec_add_epi16(value, weight));
    }
#else
    for (int i = 0; i < L1_SIZE; ++i)
        colors[side][i] += weights[i];
#endif
}

void NNUE::Accumulator::movePiece(const Square kingSquare, const int side, const int piece,
                                  const Square from, const Square to) {
    const int16_t* fromWeights = featureAddress(kingSquare, side, piece, from);
    const int16_t* toWeights = featureAddress(kingSquare, side, piece, to);
#if defined(USE_SIMD)
    for (int i = 0; i < L1_SIZE; i += FT_CHUNK_SIZE) {
        const vepi16 value = vec_load_epi(reinterpret_cast<const vepi16*>(&colors[side][i]));
        const vepi16 removed = vec_load_epi(reinterpret_cast<const vepi16*>(&fromWeights[i]));
        const vepi16 added = vec_load_epi(reinterpret_cast<const vepi16*>(&toWeights[i]));
        vec_store_epi(reinterpret_cast<vepi16*>(&colors[side][i]),
                      vec_add_epi16(vec_sub_epi16(value, removed), added));
    }
#else
    for (int i = 0; i < L1_SIZE; ++i)
        colors[side][i] += toWeights[i] - fromWeights[i];
#endif
}

void NNUE::Accumulator::removePiece(const Square kingSquare, const int side, const int piece, const Square square) {
    const int16_t* weights = featureAddress(kingSquare, side, piece, square);
#if defined(USE_SIMD)
    for (int i = 0; i < L1_SIZE; i += FT_CHUNK_SIZE) {
        const vepi16 value = vec_load_epi(reinterpret_cast<const vepi16*>(&colors[side][i]));
        const vepi16 weight = vec_load_epi(reinterpret_cast<const vepi16*>(&weights[i]));
        vec_store_epi(reinterpret_cast<vepi16*>(&colors[side][i]), vec_sub_epi16(value, weight));
    }
#else
    for (int i = 0; i < L1_SIZE; ++i)
        colors[side][i] -= weights[i];
#endif
}

void NNUE::Accumulator::doUpdates(const Square kingSquare, const int side, const Accumulator& input) {
    const DirtyPieces& dirty = dirtyPieces;
    const int16_t* sub0 = dirty.type == DirtyPieces::NONE ? nullptr
                        : featureAddress(kingSquare, side, dirty.sub0.piece, dirty.sub0.square);
    const int16_t* add0 = dirty.type == DirtyPieces::NONE ? nullptr
                        : featureAddress(kingSquare, side, dirty.add0.piece, dirty.add0.square);
    const int16_t* sub1 = dirty.type == DirtyPieces::CAPTURE || dirty.type == DirtyPieces::CASTLING
                        ? featureAddress(kingSquare, side, dirty.sub1.piece, dirty.sub1.square) : nullptr;
    const int16_t* add1 = dirty.type == DirtyPieces::CASTLING
                        ? featureAddress(kingSquare, side, dirty.add1.piece, dirty.add1.square) : nullptr;

#if defined(USE_SIMD)
    for (int i = 0; i < L1_SIZE; i += FT_CHUNK_SIZE) {
        vepi16 value = vec_load_epi(reinterpret_cast<const vepi16*>(&input.colors[side][i]));
        if (sub0) {
            value = vec_sub_epi16(value, vec_load_epi(reinterpret_cast<const vepi16*>(&sub0[i])));
            value = vec_add_epi16(value, vec_load_epi(reinterpret_cast<const vepi16*>(&add0[i])));
        }
        if (sub1)
            value = vec_sub_epi16(value, vec_load_epi(reinterpret_cast<const vepi16*>(&sub1[i])));
        if (add1)
            value = vec_add_epi16(value, vec_load_epi(reinterpret_cast<const vepi16*>(&add1[i])));
        vec_store_epi(reinterpret_cast<vepi16*>(&colors[side][i]), value);
    }
#else
    for (int i = 0; i < L1_SIZE; ++i) {
        colors[side][i] = input.colors[side][i];
        if (sub0)
            colors[side][i] += add0[i] - sub0[i];
        if (sub1)
            colors[side][i] -= sub1[i];
        if (add1)
            colors[side][i] += add1[i];
    }
#endif
    updated[side] = true;
}

void NNUE::Accumulator::reset(const int side) {
    std::memcpy(colors[side].data(), net->FTBiases, sizeof(net->FTBiases));
}

void NNUE::Accumulator::refresh(Position* pos, const int side) {
    reset(side);
    const Square king = KingSQ(pos, side);
    for (int piece = WP; piece <= BK; ++piece) {
        Bitboard pieces = pos->state().bitboards[piece];
        while (pieces)
            addPiece(king, side, piece, static_cast<Square>(popLsb(pieces)));
    }
    updated[side] = true;
}

void NNUE::FinnyEntry::reset() {
    std::memset(occupancies, 0, sizeof(occupancies));
    accumulator.reset(WHITE);
    accumulator.reset(BLACK);
    accumulator.updated[WHITE] = accumulator.updated[BLACK] = true;
}

bool NNUE::needRefresh(const int side, const Square oldKing, const Square newKing) {
    if (oldKing == no_sq || (get_file[oldKing] > 3) != (get_file[newKing] > 3))
        return true;
    return getBucket(oldKing, side) != getBucket(newKing, side);
}

void NNUE::FinnyTable::reset() {
    initialized = false;
    rootKey = 0;
    for (auto& row : entries)
        for (auto& entry : row)
            entry.reset();
}

void NNUE::FinnyTable::refresh(Position* pos, Accumulator& accumulator, const int side) {
    const Square king = KingSQ(pos, side);
    const bool flip = get_file[king] > 3;
    const int bucket = getBucket(king, side);
    FinnyEntry& entry = entries[flip][bucket];

    for (int piece = WP; piece <= BK; ++piece) {
        Bitboard removed = entry.occupancies[side][piece] & ~pos->state().bitboards[piece];
        Bitboard added = pos->state().bitboards[piece] & ~entry.occupancies[side][piece];
        while (removed && added)
            entry.accumulator.movePiece(king, side, piece,
                                        static_cast<Square>(popLsb(removed)),
                                        static_cast<Square>(popLsb(added)));
        while (removed)
            entry.accumulator.removePiece(king, side, piece, static_cast<Square>(popLsb(removed)));
        while (added)
            entry.accumulator.addPiece(king, side, piece, static_cast<Square>(popLsb(added)));
        entry.occupancies[side][piece] = pos->state().bitboards[piece];
    }

    accumulator.colors[side] = entry.accumulator.colors[side];
    accumulator.updated[side] = true;
}

void NNUE::FinnyTable::update(Position* pos, Accumulator& accumulator, const int head) {
    for (int side = WHITE; side <= BLACK; ++side) {
        if (accumulator.updated[side])
            continue;

        const Square king = accumulator.kings[side];
        int index = head;
        while (index > 0) {
            Accumulator& previous = accumulatorStack[index - 1];
            if (NNUE::needRefresh(side, previous.kings[side], king)) {
                refresh(pos, accumulator, side);
                break;
            }
            if (previous.updated[side]) {
                while (index <= head) {
                    accumulatorStack[index].doUpdates(king, side, accumulatorStack[index - 1]);
                    ++index;
                }
                break;
            }
            --index;
        }
    }
}

NNUE::Accumulator& NNUE::FinnyTable::prepare(Position* pos) {
    const int head = pos->history.head;
    const ZobristKey currentRootKey = pos->history.boardStateHistory[0].posKey;
    if (!initialized || rootKey != currentRootKey) {
        reset();
        Accumulator& root = accumulatorStack[0];
        pos->history.head = 0;
        root.refresh(pos, WHITE);
        root.refresh(pos, BLACK);
        root.kings[WHITE] = KingSQ(pos, WHITE);
        root.kings[BLACK] = KingSQ(pos, BLACK);
        root.key = pos->getPoskey();
        rootKey = root.key;
        pos->history.head = head;
        initialized = true;
        if (head == 0)
            return root;
    }

    for (int index = 1; index <= head; ++index) {
        const BoardState& state = pos->history.boardStateHistory[index];
        Accumulator& accumulator = accumulatorStack[index];
        if (accumulator.key != state.posKey)
            accumulator.updated[WHITE] = accumulator.updated[BLACK] = false;
        accumulator.kings[WHITE] = kingSquare(state, WHITE);
        accumulator.kings[BLACK] = kingSquare(state, BLACK);
        accumulator.dirtyPieces = state.dirtyPieces;
        accumulator.key = state.posKey;
    }

    Accumulator& accumulator = accumulatorStack[head];
    update(pos, accumulator, head);
    return accumulator;
}

// does FT activate for one pov at a time
void NNUE::povActivateAffine(const Accumulator& accumulator, const int side, uint16_t *base,
                             uint16_t *nnzIndices, int &nnzCount, uint8_t *output) {
    const PovAccumulator& accumCache = accumulator.colors[side];

#if defined(USE_SIMD)
    const vepi16 Zero = vec_zero_epi16();
    const vepi16 One = vec_set1_epi16(FT_QUANT);
    v128i baseVec = vec128_loadu_epi16(reinterpret_cast<const v128i*>(base));
    for (int i = 0; i < L1_SIZE / 2; i += 2 * FT_CHUNK_SIZE) {
        const vepi16 input0a = vec_load_epi(reinterpret_cast<const vepi16 *>(&accumCache[i + 0 + 0]));
        const vepi16 input0b = vec_load_epi(reinterpret_cast<const vepi16 *>(&accumCache[i + FT_CHUNK_SIZE + 0]));
        const vepi16 input1a = vec_load_epi(reinterpret_cast<const vepi16 *>(&accumCache[i + 0 + L1_SIZE / 2]));
        const vepi16 input1b = vec_load_epi(reinterpret_cast<const vepi16 *>(&accumCache[i + FT_CHUNK_SIZE + L1_SIZE / 2]));

        // Comments stolen from SF (since I was the original author of this anyways):
        // What we want to do is multiply inputs in a pairwise manner (after clipping), and then shift right by FT_SHIFT. Instead, we
        // shift left by (16 - FT_SHIFT), and use mulhi, stripping the bottom 16 bits, effectively shifting right by 16, resulting in a net shift
        // of FT_SHIFT bits. We use mulhi because it maintains the sign of the multiplication (unlike mullo), allowing us to make use
        // of packus to clip 2 of the inputs, resulting in a save of 2 "vec_max_epi16" calls.
        const vepi16 clipped0a = vec_min_epi16(vec_max_epi16(input0a, Zero), One);
        const vepi16 clipped0b = vec_min_epi16(vec_max_epi16(input0b, Zero), One);
        const vepi16 clipped1a = vec_min_epi16(input1a, One);
        const vepi16 clipped1b = vec_min_epi16(input1b, One);

        const vepi16 producta = vec_mulhi_epi16(vec_slli_epi16(clipped0a, 16 - FT_SHIFT), clipped1a);
        const vepi16 productb = vec_mulhi_epi16(vec_slli_epi16(clipped0b, 16 - FT_SHIFT), clipped1b);

        const vepi8 product = vec_packus_epi16(producta, productb);
        vec_store_epi(reinterpret_cast<vepi8 *>(&output[i]), product);
        const v128i LookupIncr = vec128_set1_epi16(8);
        // store all non zero indices to transform L1 sparsely
        // start ny creating a mask masking all the non 0 elements in our product vector (actually 4 8bit elements
        // creating a 32 bit element at a time, which will be non 0 if at least 1 8 bit element is.
        const uint16_t nnzMask = vec_nnz_mask(product);
        // check number of elements inside the actual register / 8 since we are working on a per bit basis
        for (int lookup = 0; lookup < int(sizeof(vepi32) / sizeof(uint32_t)) / 8; ++lookup) {
            // 0-255 mask index for the table
            uint8_t maskSlice = (nnzMask >> (8 * lookup)) & 0xFF;
            // look up from a precaculated table how many bits are set to 1 and what the indexes are
            NNZEntry nnzEntry = nnzTable.table[maskSlice];
            // get ready to store in in nnzIndices by getting the appropriate pointer to it
            v128i* nnzStore   = reinterpret_cast<v128i*>(&nnzIndices[nnzCount]);
            // add entry indices to our non-zero indices list
            const v128i indices = vec128_loadu_epi16(reinterpret_cast<const v128i*>(nnzEntry.indices));
            // add base address to indexes and store them
            vec128_storeu_epi16(nnzStore, vec128_add_epi16(baseVec, indices));

            // increment count of total non 0 elements
            nnzCount += nnzEntry.count;
            // update base value for the next cycle iteration
            baseVec = vec128_add_epi16(baseVec, LookupIncr);
        }
    }
    vec128_storeu_epi16(reinterpret_cast<v128i*>(base), baseVec);
#else
    for (int i = 0; i < L1_SIZE / 2; ++i) {
        int16_t clipped0 = std::clamp<int16_t>(accumCache[i], 0, FT_QUANT);
        int16_t clipped1 = std::clamp<int16_t>(accumCache[i + L1_SIZE / 2], 0, FT_QUANT);
        output[i] = static_cast<uint8_t>(clipped0 * clipped1 >> FT_SHIFT);
    }
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
            sums[j] += static_cast<int32_t>(inputs[i] * weights[i * L2_SIZE + j]);
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
        vec_store_ps(&output[i * L3_CHUNK_SIZE], clipped);
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
        output[i] = std::clamp(sums[i], 0.0f, 1.0f);
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

void NNUE::activateAffine(const Accumulator& accumulator, const int sideToMove,
                          [[maybe_unused]] uint16_t *base, [[maybe_unused]] uint16_t *nnzIndices,
                          [[maybe_unused]] int &nnzCount, uint8_t *output) {
    povActivateAffine(accumulator, sideToMove, base, nnzIndices, nnzCount, output);
    povActivateAffine(accumulator, sideToMove ^ 1, base, nnzIndices, nnzCount, &output[L1_SIZE / 2]);
}

int NNUE::output(Position *pos, NNUE::FinnyTable *FinnyPointer) {
    int nnzCount = 0;
    uint16_t base[8] = {}; // replaces v128i base
    alignas (64) uint16_t nnzIndices[L1_SIZE / L1_CHUNK_PER_32];

    const int pieceCount = pos->PieceCount();
    constexpr int bucketDivisor = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;
    const int outputBucket = (pieceCount - 2) / bucketDivisor;
    alignas (64) uint8_t FTOutputs[L1_SIZE];
    alignas (64) float L1Outputs[EFFECTIVE_L2_SIZE];
    alignas (64) float L2Outputs[L3_SIZE];
    float L3Output;

    const Accumulator& accumulator = FinnyPointer->prepare(pos);
#ifndef NDEBUG
    Accumulator refreshed;
    refreshed.refresh(pos, WHITE);
    refreshed.refresh(pos, BLACK);
    assert(accumulator.colors[WHITE] == refreshed.colors[WHITE]);
    assert(accumulator.colors[BLACK] == refreshed.colors[BLACK]);
#endif
    activateAffine(accumulator, pos->side, base, nnzIndices, nnzCount, FTOutputs);

    propagateL1(FTOutputs, nnzIndices, nnzCount,
#if defined(USE_SIMD)
                net->L1WeightsAlt[outputBucket],
#else
                &net->L1Weights[outputBucket][0][0],
#endif
                net->L1Biases[outputBucket], L1Outputs);

    propagateL2(L1Outputs, &net->L2Weights[outputBucket][0][0], net->L2Biases[outputBucket], L2Outputs);

    propagateL3(L2Outputs, net->L3Weights[outputBucket], net->L3Biases[outputBucket], L3Output);

    return L3Output * NET_SCALE;
}

size_t NNUE::getIndex(const int piece, const int square, const int side, const int bucket, const bool flip) {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColorPov = Color[piece] ^ side;

    // Get the final indexes of the updates, accounting for hm
    auto squarePov = square ^ (0b111'000 * !side) ^ (0b000'111 * flip);
    auto idx = bucket * NUM_INPUTS + pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return idx * L1_SIZE;
}
