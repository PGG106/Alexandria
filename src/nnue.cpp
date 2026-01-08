#include "nnue.h"
#include "simd.h"
#include <algorithm>
#include "position.h"
#include <cstdint>
#include <cstring>
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

const Network* net;

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

    // Transform the quantised weights and biases into the form we want for optimal inference
    // FT Weights
    for (int i = 0; i < INPUT_BUCKETS * NUM_INPUTS * L1_SIZE; ++i)
        permutedNet.FTWeights[i] = quantisedNet.FTWeights[i];

    // FT Biases
    for (int i = 0; i < L1_SIZE; ++i)
        permutedNet.FTBiases[i] = quantisedNet.FTBiases[i];


    // Transpose FT weights and biases so that packus transposes it back to the intended order
#if defined(USE_SIMD)
    __m128i *weight = reinterpret_cast<__m128i*>(permutedNet.FTWeights);
    __m128i *biases = reinterpret_cast<__m128i*>(permutedNet.FTBiases);
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
    for (int i = 0; i < INPUT_BUCKETS * NUM_INPUTS * L1_SIZE / numChunks; i += numRegi) {
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
                    permutedNet.L1Weights[bucket][  i * L1_CHUNK_PER_32 * L2_SIZE
                                          + j * L1_CHUNK_PER_32
                                          + k] = quantisedNet.L1Weights[i * L1_CHUNK_PER_32 + k][bucket][j];
#else
        for (int i = 0; i < L1_SIZE; ++i)
            for (int j = 0; j < L2_SIZE; ++j)
                permutedNet.L1Weights[bucket][j * L1_SIZE + i] = quantisedNet.L1Weights[i][bucket][j];
#endif

        // Transpose L1 Biases
        for (int i = 0; i < L2_SIZE; ++i)
            permutedNet.L1Biases[bucket][i] = quantisedNet.L1Biases[bucket][i];

        // Transpose L2 Weights
        for (int i = 0; i < L2_SIZE; ++i)
            for (int j = 0; j < L3_SIZE; ++j)
                permutedNet.L2Weights[bucket][i * L3_SIZE + j] = quantisedNet.L2Weights[i][bucket][j];

        // Transpose L2 Biases
        for (int i = 0; i < L3_SIZE; ++i)
            permutedNet.L2Biases[bucket][i] = quantisedNet.L2Biases[bucket][i];

        // Transpose L3 Weights
        for (int i = 0; i < L3_SIZE; ++i)
            permutedNet.L3Weights[bucket][i] = quantisedNet.L3Weights[i][bucket];

        // Transpose L3 Biases
        permutedNet.L3Biases[bucket] = quantisedNet.L3Biases[bucket];
    }


    std::ofstream out{"nn.net", std::ios::binary};
    out.write(reinterpret_cast<const char *>(&permutedNet), sizeof(Network));
    exit(12);
}

void NNUE::init() {
    net = reinterpret_cast<const Network*>(gEVALData);
}

// does FT activate for one pov at a time
void NNUE::povActivateAffine(Position *pos, NNUE::FinnyTable *FinnyPointer, const int side, uint8_t *output) {
    const int kingSq = KingSQ(pos, side);
    const bool flip = get_file[kingSq] > 3;
    const int kingBucket = getBucket(kingSq, side);
    FinnyTableEntry &cachedEntry = (*FinnyPointer)[side][kingBucket][flip];

    size_t add[32], remove[32]; // Max add or remove is 32 unless illegal position
    size_t addCnt = 0, removeCnt = 0;

    for (int piece = WP; piece <= BK; piece++) {
        Bitboard added = pos->state().bitboards[piece] & ~cachedEntry.occupancies[piece];
        Bitboard removed = cachedEntry.occupancies[piece] & ~pos->state().bitboards[piece];
        while (added) {
            int square = popLsb(added);
            add[addCnt++] = getIndex(piece, square, side, kingBucket, flip);
        }

        while (removed) {
            int square = popLsb(removed);
            remove[removeCnt++] = getIndex(piece, square, side, kingBucket, flip);
        }

        cachedEntry.occupancies[piece] = pos->state().bitboards[piece];
    }


    NNUE::PovAccumulator &accumCache = cachedEntry.accumCache;

    for (size_t i = 0; i < addCnt; i++) {
        const auto added = add[i];
        for (int j = 0; j < L1_SIZE; ++j) {
            accumCache[j] += net->FTWeights[added + j];
        }
    }

    for (size_t i = 0; i < removeCnt; i++) {
        const auto removed = remove[i];
        for (int j = 0; j < L1_SIZE; ++j) {
            accumCache[j] -= net->FTWeights[removed + j];
        }
    }
#if defined(USE_SIMD)
    const vepi16 Zero = vec_zero_epi16();
    const vepi16 One  = vec_set1_epi16(FT_QUANT);
    for (int i = 0; i < L1_SIZE / 2; i += 2 * FT_CHUNK_SIZE) {
        const vepi16 input0a   = vec_load_epi(reinterpret_cast<const vepi16*>(&accumCache[i + 0             + 0]));
        const vepi16 input0b   = vec_load_epi(reinterpret_cast<const vepi16*>(&accumCache[i + FT_CHUNK_SIZE + 0]));
        const vepi16 input1a   = vec_load_epi(reinterpret_cast<const vepi16*>(&accumCache[i + 0             + L1_SIZE / 2]));
        const vepi16 input1b   = vec_load_epi(reinterpret_cast<const vepi16*>(&accumCache[i + FT_CHUNK_SIZE + L1_SIZE / 2]));

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

        const vepi8  product   = vec_packus_epi16(producta, productb);
        vec_store_epi(reinterpret_cast<vepi8*>(&output[i]), product);
    }
#else
    for (int i = 0; i < L1_SIZE / 2; ++i) {
        int16_t clipped0 = std::clamp<int16_t>(accumCache[i], 0, FT_QUANT);
        int16_t clipped1 = std::clamp<int16_t>(accumCache[i + L1_SIZE / 2], 0, FT_QUANT);
        output[i] = static_cast<uint8_t>(clipped0 * clipped1 >> FT_SHIFT);
    }
#endif
}

void NNUE::propagateL1(const uint8_t *inputs, const int8_t *weights, const float *biases, float *output) {
#if defined(USE_SIMD)
    vepi32 sums[L2_SIZE / L2_CHUNK_SIZE] = {};
    const int32_t *inputs32 = reinterpret_cast<const int32_t*>(inputs);

    // We read in the inputs in chunks of 4 (as dpbusd horizontally sums by 4).
    // Then, each chunk of 4 is multiplied by the L1 weights. (The weights are pre-permuted to allow us to do this)
    // We also unroll by 2 to save a madd every 2 multiplications (in the non VNNI case).
    // Note that we sacrificed some quantisation accuracy to do this, as the additional accuracy had no elo gain.
    int i = 0;
    for (; i + 1 < L1_SIZE / L1_CHUNK_PER_32; i += 2){
        const uint16_t indexa = i;
        const uint16_t indexb = i + 1;
        const vepi32 input32a = vec_set1_epi32(inputs32[indexa]);
        const vepi32 input32b = vec_set1_epi32(inputs32[indexb]);
        const vepi8 *weighta  = reinterpret_cast<const vepi8*>(&weights[indexa * L1_CHUNK_PER_32 * L2_SIZE]);
        const vepi8 *weightb  = reinterpret_cast<const vepi8*>(&weights[indexb * L1_CHUNK_PER_32 * L2_SIZE]);
        for (int j = 0; j < L2_SIZE / L2_CHUNK_SIZE; ++j)
            sums[j] = vec_dpbusdx2_epi32(sums[j], input32a, weighta[j], input32b, weightb[j]);
    }

    for (; i < L1_SIZE / L1_CHUNK_PER_32; ++i) {
        const uint16_t index = i;
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
        const vps32 One     = vec_set1_ps(1.0f);
        const vps32 clipped = vec_min_ps(vec_max_ps(sumPs, Zero), One);
        const vps32 squared = vec_mul_ps(clipped, clipped);
        vec_store_ps(&output[i * L2_CHUNK_SIZE], squared);
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
        const float clipped = std::clamp(float(sums[i]) * L1_MUL + biases[i], 0.0f, 1.0f);
        const float squared = clipped * clipped;
        output[i] = squared;
    }
#endif
}

void NNUE::propagateL2(const float *inputs, const float *weights, const float *biases, float *output) {

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
        const vps32 Zero    = vec_zero_ps();
        const vps32 One     = vec_set1_ps(1.0f);
        const vps32 clipped = vec_min_ps(vec_max_ps(sumVecs[i], Zero), One);
        const vps32 squared = vec_mul_ps(clipped, clipped);
        vec_store_ps(&output[i * L3_CHUNK_SIZE], squared);
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

void NNUE::activateAffine(Position *pos, NNUE::FinnyTable *FinnyPointer, uint8_t *output) {
    povActivateAffine(pos, FinnyPointer, pos->side, output);
    povActivateAffine(pos, FinnyPointer, pos->side ^ 1, &output[L1_SIZE / 2]);
}

int NNUE::output(Position *pos, NNUE::FinnyTable *FinnyPointer) {
    const int pieceCount = pos->PieceCount();
    const int outputBucket = std::min((63 - pieceCount) * (32 - pieceCount) / 225, 7);
    alignas (64) uint8_t FTOutputs[L1_SIZE];
    alignas (64) float L1Outputs[L2_SIZE];
    alignas (64) float L2Outputs[L3_SIZE];
    float L3Output;

    // does FT activation for both accumulators
    activateAffine(pos, FinnyPointer, FTOutputs);

    propagateL1(FTOutputs, net->L1Weights[outputBucket], net->L1Biases[outputBucket], L1Outputs);

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
