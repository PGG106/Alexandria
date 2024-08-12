#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <cmath>
#include <memory>
#include "bitboard.h"
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

#if NETUP
UnquantisedNetwork unquantisedNet;
NNZData nnzData;
#endif

QuantisedNetwork quantisedNet;
Network net;
NNZTable nnzTable;

// Thanks to Disservin for having me look at his code and Luecx for the
// invaluable help and the immense patience

void NNUE::init(const char *file) {

    // open the nn file
    std::ifstream stream{file, std::ios::binary};

    #if NETUP
    // Read unquantised net
    if (!stream)
        unquantisedNet = *reinterpret_cast<const UnquantisedNetwork*>(gEVALData);
    else
        stream.read(reinterpret_cast<char *>(&unquantisedNet), sizeof(UnquantisedNetwork));

    // Then, quantise the net below

    // Quantise FT Weights
    for (int i = 0; i < NUM_INPUTS * L1_SIZE; ++i)
        quantisedNet.FTWeights[i] = static_cast<int16_t>(std::round(unquantisedNet.FTWeights[i] * FT_QUANT));

    // Quantise FT Biases
    for (int i = 0; i < L1_SIZE; ++i)
        quantisedNet.FTBiases[i] = static_cast<int16_t>(std::round(unquantisedNet.FTBiases[i] * FT_QUANT));

    // Quantise L1, L2 and L3 weights and biases
    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket) {

        // Quantise L1 Weights
        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                quantisedNet.L1Weights[i][bucket][j] = static_cast<int8_t>(std::round(unquantisedNet.L1Weights[i][bucket][j] * L1_QUANT));

        // Quantise L1 Biases
        for (int i = 0; i < L2_SIZE; ++i)
            quantisedNet.L1Biases[bucket][i] = unquantisedNet.L1Biases[bucket][i];

        // Quantise L2 Weights
        for (int i = 0; i < L2_SIZE; ++i)
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
    #else
    // Read the quantised net
    if (!stream)
        quantisedNet = *reinterpret_cast<const QuantisedNetwork*>(gEVALData);
    else
        stream.read(reinterpret_cast<char *>(&quantisedNet), sizeof(QuantisedNetwork));
    #endif

    // Transform the quantised weights and biases into the form we want for optimal inference
    // FT Weights
    for (int i = 0; i < NUM_INPUTS * L1_SIZE; ++i)
        net.FTWeights[i] = quantisedNet.FTWeights[i];

    // FT Biases
    for (int i = 0; i < L1_SIZE; ++i)
        net.FTBiases[i] = quantisedNet.FTBiases[i];

    // Transpose FT weights and biases so that packus transposes it back to the intended order
    #if defined(USE_SIMD)
    __m128i *weight = reinterpret_cast<__m128i*>(net.FTWeights);
    __m128i *biases = reinterpret_cast<__m128i*>(net.FTBiases);
    constexpr int numChunks = sizeof(__m128i) / sizeof(int16_t);

    #if defined(USE_AVX512)
    // 0, 1, 2, 3, 4, 5, 6, 7 -> 0, 2, 4, 6, 1, 3, 5, 7
    constexpr int numRegi = 8;
    constexpr int order[numRegi] = {0, 2, 4, 6, 1, 3, 5, 7};
    #elif defined(USE_AVX2)
    // 0, 1, 2, 3 -> 0, 2, 1, 3
    constexpr int numRegi = 4;
    constexpr int order[numRegi] = {0, 2, 1, 3};
    #endif

    __m128i regi[numRegi];

    // Transpose weights
    for (int i = 0; i < NUM_INPUTS * L1_SIZE / numChunks; i += numRegi) {
        for (int j = 0; j < numRegi; ++j)
            regi[j] = weight[i + j];

        for (int j = 0; j < numRegi; ++j)
            weight[i + j] = regi[order[j]];
    }

    // Transpose biases
    for (int i = 0; i < L1_SIZE / numChunks; i += numRegi) {
        for (int j = 0; j < numRegi; ++j)
            regi[j] = biases[i + j];

        for (int j = 0; j < numRegi; ++j)
            biases[i + j] = regi[order[j]];
    }
    #endif

    // Transpose L1, L2 and L3 weights and biases
    for (int bucket = 0; bucket < OUTPUT_BUCKETS; ++bucket) {

        // Transpose L1 weights
        #if defined(USE_SIMD)
        for (int i = 0; i < L1_SIZE / L1_CHUNK_PER_32; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                for (int k = 0; k < L1_CHUNK_PER_32; ++k)
                    net.L1Weights[bucket][  i * L1_CHUNK_PER_32 * L2_SIZE
                                          + j * L1_CHUNK_PER_32
                                          + k] = quantisedNet.L1Weights[i * L1_CHUNK_PER_32 + k][bucket][j];
        #else
        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                net.L1Weights[bucket][j * L1_SIZE + i] = quantisedNet.L1Weights[i][bucket][j];
        #endif

        // Transpose L1 Biases
        for (int i = 0; i < L2_SIZE; ++i)
            net.L1Biases[bucket][i] = quantisedNet.L1Biases[bucket][i];

        // Transpose L2 Weights
        for (int i = 0; i < L2_SIZE; ++i)
            for (int j = 0; j < L3_SIZE; ++j)
                net.L2Weights[bucket][i * L3_SIZE + j] = quantisedNet.L2Weights[i][bucket][j];

        // Transpose L2 Biases
        for (int i = 0; i < L3_SIZE; ++i)
            net.L2Biases[bucket][i] = quantisedNet.L2Biases[bucket][i];

        // Transpose L3 Weights
        for (int i = 0; i < L3_SIZE; ++i)
            net.L3Weights[bucket][i] = quantisedNet.L3Weights[i][bucket];

        // Transpose L3 Biases
        net.L3Biases[bucket] = quantisedNet.L3Biases[bucket];
    }
}

void NNUE::finish_netup([[maybe_unused]] const char *file) {
    #if NETUP
    nnzData.permute(quantisedNet);

    // Write the net to file
    std::ofstream out{file, std::ios::binary};

    if (!out) {
        std::cout << "Error writing quantised net data" << std::endl;
    }
    else {
        out.write(reinterpret_cast<const char *>(&quantisedNet), sizeof(QuantisedNetwork));
    }

    #endif
}

void NNUE::update(Accumulator *acc, Position *pos) {
    for (int pov = WHITE; pov <= BLACK; pov++) {

        auto &povAccumulator = (acc)->perspective[pov];

        // return early if we already updated this accumulator (aka it's "clean"), we can use pending adds to check if it has pending changes (any change will result in at least one add)
        if (povAccumulator.isClean())
            continue;

        auto &previousPovAccumulator = (acc - 1)->perspective[pov];
        // if this accumulator is in need of a refresh or the previous one is clean, we can just start updating
        const bool isUsable = povAccumulator.needsRefresh || previousPovAccumulator.isClean();
        // if we can't update we need to start scanning backwards
        // if in our scan we'll find an accumulator that needs a refresh we'll just refresh the top one, this saves us from having to store board states
        bool shouldRefresh = false;

        if (!isUsable) {
            int UEableAccs;
            bool shouldUE = false;
            for (UEableAccs = 1; UEableAccs < MAXPLY; UEableAccs++) {
                auto &currentAcc = (acc - UEableAccs)->perspective[pov];
                // check if the current acc should be refreshed
                shouldRefresh = currentAcc.needsRefresh;
                if (shouldRefresh) break;
                    // check if the current acc can be used as a starting point for an UE chain
                else if ((acc - UEableAccs - 1)->perspective[pov].isClean()) {
                    shouldUE = true;
                    break;
                }
            }
            if (shouldUE) {
                for (int j = (pos->accumStackHead - 1 - UEableAccs); j <= pos->accumStackHead - 1; j++) {
                    pos->accumStack[j].perspective[pov].applyUpdate(pos->accumStack[j - 1].perspective[pov]);
                }
            }
        }

        // if we are here we either have an up to date accumulator we can UE on top of or we one we need to refresh
        if (povAccumulator.needsRefresh || shouldRefresh) {
            povAccumulator.accumulate(pos);
            // Reset the add and sub vectors for this accumulator, this will make it "clean" for future updates
            povAccumulator.NNUEAdd.clear();
            povAccumulator.NNUESub.clear();
            // mark any accumulator as refreshed
            povAccumulator.needsRefresh = false;
        } else {
            povAccumulator.applyUpdate(previousPovAccumulator);
        }
    }
}

void NNUE::Pov_Accumulator::addSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub) {
        const auto Add = &net.FTWeights[add * L1_SIZE];
        const auto Sub = &net.FTWeights[sub * L1_SIZE];
        for (int i = 0; i < L1_SIZE; i++) {
            this->values[i] = prev_acc.values[i] - Sub[i] + Add[i];
        }
}

void NNUE::Pov_Accumulator::addSubSub(NNUE::Pov_Accumulator &prev_acc, std::size_t add, std::size_t sub1, std::size_t sub2) {
        auto Add = &net.FTWeights[add * L1_SIZE];
        auto Sub1 = &net.FTWeights[sub1 * L1_SIZE];
        auto Sub2 = &net.FTWeights[sub2 * L1_SIZE];
        for (int i = 0; i < L1_SIZE; i++) {
            this->values[i] =  prev_acc.values[i] - Sub1[i] - Sub2[i] + Add[i];
        }
}

void NNUE::accumulate(NNUE::Accumulator& board_accumulator, Position* pos) {
    for(auto& pov_acc : board_accumulator.perspective) {
        pov_acc.accumulate(pos);
    }
}

void NNUE::Pov_Accumulator::applyUpdate(NNUE::Pov_Accumulator& previousPovAccumulator) {

    assert(previousPovAccumulator.isClean());

    // return early if we already updated this accumulator (aka it's "clean"), we can use pending adds to check if it has pending changes (any change will result in at least one add)
    if (this->isClean())
        return;

    // figure out what update we need to apply and do that
    int adds = NNUEAdd.size();
    int subs = NNUESub.size();

    // Quiets
    if (adds == 1 && subs == 1) {
        this->addSub( previousPovAccumulator, this->NNUEAdd[0], this->NNUESub[0]);
    }
    // Captures
    else if (adds == 1 && subs == 2) {
        this->addSubSub(previousPovAccumulator, this->NNUEAdd[0], this->NNUESub[0],this->NNUESub[1]);
    }
    // Castling
    else {
        this->addSub( previousPovAccumulator, this->NNUEAdd[0], this->NNUESub[0]);
        this->addSub(*this, this->NNUEAdd[1], this->NNUESub[1]);
        // Note that for second addSub, we put acc instead of acc - 1 because we are updating on top of
        // the half-updated accumulator
    }

    // Reset the add and sub vectors for this accumulator, this will make it "clean" for future updates
    this->NNUEAdd.clear();
    this->NNUESub.clear();
}

void NNUE::Pov_Accumulator::accumulate(Position *pos) {
    for (int i = 0; i < L1_SIZE; i++) {
       values[i] = net.FTBiases[i];
    }

    const bool flip = get_file[KingSQ(pos, pov)] > 3;
    Bitboard occ = pos->Occupancy(BOTH);

    while (occ) {
        const int square = popLsb(occ);
        const int piece = pos->PieceOn(square);
        auto Idx = GetIndex(piece, square, flip);
        auto Add = &net.FTWeights[Idx * L1_SIZE];
        for (int j = 0; j < L1_SIZE; j++) {
            values[j] += Add[j];
        }
    }
}

int NNUE::Pov_Accumulator::GetIndex(const int piece, const int square, bool flip) const {
    constexpr std::size_t COLOR_STRIDE = 64 * 6;
    constexpr std::size_t PIECE_STRIDE = 64;
    const int piecetype = GetPieceType(piece);
    const int pieceColor = Color[piece];
    auto pieceColorPov = pov == WHITE ? pieceColor : (1 ^ pieceColor);
    // Get the final indexes of the updates, accounting for hm
    auto squarePov = pov == WHITE ? (square ^ 0b111'000) : square;
    if(flip) squarePov ^= 0b000'111;
    std::size_t Idx = pieceColorPov * COLOR_STRIDE + piecetype * PIECE_STRIDE + squarePov;
    return Idx;
}


void NNUE::ActivateFT(const int16_t *us, const int16_t *them, [[maybe_unused]] uint16_t *nnzIndices, [[maybe_unused]] int &nnzCount, uint8_t *output) {
    #if defined(USE_SIMD)
    int offset = 0;
    const vepi16 Zero = vec_zero_epi16();
    const vepi16 One  = vec_set1_epi16(FT_QUANT);

    nnzCount = 0;
    v128i base = vec128_zero_epi16();
    const v128i LookupIncr = vec128_set1_epi16(8);
    for (const int16_t *acc : {us, them}) {
        for (int i = 0; i < L1_SIZE / 2; i += 2 * FT_CHUNK_SIZE) {
            const vepi16 input0a   = vec_load_epi(reinterpret_cast<const vepi16*>(&acc[i + 0             + 0]));
            const vepi16 input0b   = vec_load_epi(reinterpret_cast<const vepi16*>(&acc[i + FT_CHUNK_SIZE + 0]));
            const vepi16 input1a   = vec_load_epi(reinterpret_cast<const vepi16*>(&acc[i + 0             + L1_SIZE / 2]));
            const vepi16 input1b   = vec_load_epi(reinterpret_cast<const vepi16*>(&acc[i + FT_CHUNK_SIZE + L1_SIZE / 2]));

            // Comments stolen from SF (since I was the original author of this anyways):
            // What we want to do is multiply inputs in a pairwise manner (after clipping), and then shift right by FT_SHIFT. Instead, we
            // shift left by (16 - FT_SHIFT), and use mulhi, stripping the bottom 16 bits, effectively shifting right by 16, resulting in a net shift
            // of FT_SHIFT bits. We use mulhi because it maintains the sign of the multiplication (unlike mullo), allowing us to make use
            // of packus to clip 2 of the inputs, resulting in a save of 2 "vec_max_epi16" calls.
            const vepi16 clipped0a = vec_min_epi16(vec_max_epi16(input0a, Zero), One);
            const vepi16 clipped0b = vec_min_epi16(vec_max_epi16(input0b, Zero), One);
            const vepi16 clipped1a = vec_min_epi16(input1a, One);
            const vepi16 clipped1b = vec_min_epi16(input1b, One);

            const vepi16 producta  = vec_mulhi_epi16(vec_slli_epi16(clipped0a, 16 - FT_SHIFT), clipped1a);
            const vepi16 productb  = vec_mulhi_epi16(vec_slli_epi16(clipped0b, 16 - FT_SHIFT), clipped1b);

            // Note: we can skip permuting after packus because we already permuted at startup to offset this
            const vepi8  product   = vec_packus_epi16(producta, productb);
            vec_store_epi(reinterpret_cast<vepi8*>(&output[offset + i]), product);

            // What this code below does, is to store all active (nonzero) indices in the nnzIndices array,
            // to allow us to do the L1 affine transform sparsely.
            const uint16_t nnzMask = vec_nnz_mask(product);
            // We divide here since our lookup is only 8 bits
            for (int lookup = 0; lookup < int(sizeof(vepi32) / sizeof(uint32_t)) / 8; ++lookup) {

                // ########
                //         ^ we store here
                // say we have 5 nonzero indices
                // then, it becomes
                // #############
                //              ^ we store here the next time around

                uint8_t maskSlice = (nnzMask >> (8 * lookup)) & 0xFF;
                NNZEntry nnzEntry = nnzTable.table[maskSlice];
                v128i* nnzStore   = reinterpret_cast<v128i*>(&nnzIndices[nnzCount]);
                const v128i indices = vec128_loadu_epi16(reinterpret_cast<const v128i*>(nnzEntry.indices));
                vec128_storeu_epi16(nnzStore, vec128_add_epi16(base, indices));

                nnzCount += nnzEntry.count;
                base = vec128_add_epi16(base, LookupIncr);
            }
        }
        offset += L1_SIZE / 2;
    }
    #else
    int offset = 0;
    for (const int16_t *acc : {us, them}) {
        for (int i = 0; i < L1_SIZE / 2; ++i) {
            int16_t clipped0 = std::clamp<int16_t>(acc[i], 0, FT_QUANT);
            int16_t clipped1 = std::clamp<int16_t>(acc[i + L1_SIZE / 2], 0, FT_QUANT);
            output[offset + i] = static_cast<uint8_t>(clipped0 * clipped1 >> FT_SHIFT);
        }
        offset += L1_SIZE / 2;
    }
    #endif
}

void NNUE::PropagateL1(const uint8_t *inputs, [[maybe_unused]] uint16_t *nnzIndices, [[maybe_unused]] int nnzCount, const int8_t *weights, const float *biases, float *output) {
    #if defined(USE_SIMD)
    vepi32 sums[L2_SIZE / L2_CHUNK_SIZE] = {};
    const int32_t *inputs32 = reinterpret_cast<const int32_t*>(inputs);

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
        const vps32 sumMul  = vec_set1_ps(L1_MUL);
        const vps32 sumPs   = vec_mul_add_ps(vec_cvtepi32_ps(sums[i]), sumMul, biasVec);
        const vps32 Zero    = vec_zero_ps();
        vec_store_ps(&output[i * L2_CHUNK_SIZE], vec_max_ps(Zero, sumPs));
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
        output[i] = std::max(float(sums[i]) * L1_MUL + biases[i], 0.0f);
    }
    #endif
}

void NNUE::PropagateL2(const float *inputs, const float *weights, const float *biases, float *output) {
    // For each input, multiply by all the L2 weights
    #if defined(USE_SIMD)
    vps32 sumVecs[L3_SIZE / L3_CHUNK_SIZE];

    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i)
        sumVecs[i] = vec_load_ps(&biases[i * L3_CHUNK_SIZE]);

    for (int i = 0; i < L2_SIZE; ++i) {
        const vps32 inputVec = vec_set1_ps(inputs[i]);
        const vps32 *weight  = reinterpret_cast<const vps32*>(&weights[i * L3_SIZE]);
        for (int j = 0; j < L3_SIZE / L3_CHUNK_SIZE; ++j)
            sumVecs[j] = vec_mul_add_ps(inputVec, weight[j], sumVecs[j]);
    }

    // Activate L2
    for (int i = 0; i < L3_SIZE / L3_CHUNK_SIZE; ++i) {
        vec_store_ps(&output[i * L3_CHUNK_SIZE], vec_max_ps(sumVecs[i], vec_set1_ps(0.0f)));
    }
    #else
    float sums[L3_SIZE];

    for (int i = 0; i < L3_SIZE; ++i)
        sums[i] = biases[i];

    // Affine transform for L2
    for (int i = 0; i < L2_SIZE; ++i) {
        const float *weight = &weights[i * L3_SIZE];
        for (int out = 0; out < L3_SIZE; ++out) {
            sums[out] += inputs[i] * weight[out];
        }
    }

    // Activate L2
    for (int i = 0; i < L3_SIZE; ++i) {
        output[i] = std::max(sums[i], 0.0f);
    }
    #endif
}

void NNUE::PropagateL3(const float *inputs, const float *weights, const float bias, float &output) {
    // These weird multiple-sum shenanigans is to make sure we add the floats in the exact same manner
    // and order on ALL architectures, so that behaviour is deterministic
    // We multiply the weights by the inputs, and sum them up
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

// this function takes the net output for the current accumulators and returns the eval of the position
// according to the net
int32_t NNUE::output(const NNUE::Accumulator &board_accumulator, const int stm, const int outputBucket) {

    int nnzCount = 0;
    alignas (64) uint16_t nnzIndices[L1_SIZE / L1_CHUNK_PER_32];
    alignas (64) uint8_t  FTOutputs[L1_SIZE];
    alignas (64) float    L1Outputs[L2_SIZE];
    alignas (64) float    L2Outputs[L3_SIZE];
    float L3Output;

    const int16_t* us = board_accumulator.perspective[stm].values.data();
    const int16_t* them = board_accumulator.perspective[stm ^ 1].values.data();

    // Feed Forward NNUE (i.e. outputs of FT are inputs of L1, outputs of L1 are inputs of L2, etc.)
    ActivateFT (us,
                them,
                nnzIndices,
                nnzCount,
                FTOutputs);

    PropagateL1(FTOutputs,
                nnzIndices,
                nnzCount,
                net.L1Weights[outputBucket],
                net.L1Biases[outputBucket],
                L1Outputs);

    PropagateL2(L1Outputs,
                net.L2Weights[outputBucket],
                net.L2Biases[outputBucket],
                L2Outputs);

    PropagateL3(L2Outputs,
               net.L3Weights[outputBucket],
               net.L3Biases[outputBucket],
               L3Output);

    #if NETUP
    nnzData.update(FTOutputs);
    #endif

    return L3Output * NET_SCALE;
}