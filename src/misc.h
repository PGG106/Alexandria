#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include "makemove.h"
#include "io.h"
#include "eval.h"

[[nodiscard]] inline uint64_t GetTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline long long int _count, _accumulator;

// splits a string into a vector of tokens and returns it
[[nodiscard]] inline std::vector<std::string> split_command(const std::string& command) {
    std::stringstream stream(command);
    std::string intermediate;
    std::vector<std::string> tokens;

    while (std::getline(stream, intermediate, ' ')) {
        tokens.push_back(intermediate);
    }

    return tokens;
}

// returns true if in a vector of string there's one that matches the key
[[nodiscard]] inline bool Contains(const std::vector<std::string>& tokens, const std::string& key) {
    return std::find(tokens.begin(), tokens.end(), key) != tokens.end();
}

inline void dbg_mean_of(int val) { _count++; _accumulator += val; }

inline void dbg_print() { std::cout << double(_accumulator) / _count << std::endl; }
