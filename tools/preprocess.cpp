#include "../src/nnue.h"
#include "../src/simd.h"
#include <fstream>
#include <iostream>

// These will be populated from the input file
QuantisedNetwork quantisedNet;
Network permutedNet;

void permute_transpose() {
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
        for (int i = 0; i < L2_SIZE * 2; ++i)
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
}

int main(int argc, char* argv[]) {

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <infile> <outfile>\n";
        return -1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];

    std::cout << "Starting network preprocessing..." << std::endl;

    // Read the quantised network from input file
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "Error: Could not open input file: " << input_path << std::endl;
        return 1;
    }

    std::cout << "Reading quantised network from " << input_path << "..." << std::endl;

    input.read(reinterpret_cast<char*>(&quantisedNet), sizeof(QuantisedNetwork));

    if (!input) {
        std::cerr << "Error: Failed to read quantised network from input file" << std::endl;
        std::cerr << "Expected to read " << sizeof(QuantisedNetwork) << " bytes" << std::endl;
        input.close();
        return 1;
    }

    input.close();
    std::cout << "Successfully read " << sizeof(QuantisedNetwork) << " bytes" << std::endl;

    // Perform the permutation and transposition
    std::cout << "Performing permutation and transposition..." << std::endl;
    permute_transpose();
    std::cout << "Permutation complete" << std::endl;

    // Write the permuted network to output file
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        std::cerr << "Error: Could not open output file: " << output_path << std::endl;
        return 1;
    }

    std::cout << "Writing permuted network to " << output_path << "..." << std::endl;

    output.write(reinterpret_cast<const char*>(&permutedNet), sizeof(Network));

    if (!output) {
        std::cerr << "Error: Failed to write permuted network to output file" << std::endl;
        output.close();
        return 1;
    }

    output.close();

    std::cout << "Successfully preprocessed " << input_path << " -> " << output_path << std::endl;
    std::cout << "Output size: " << sizeof(Network) << " bytes" << std::endl;
    return 0;
}