#pragma once
#include <bit>
#include <cassert>
#include "types.h"

// set/get/pop bit
constexpr void set_bit(Bitboard& bitboard, const int square) { bitboard |= (1ULL << square); }
[[nodiscard]] inline int get_bit(const Bitboard bitboard, const int square) { return bitboard & (1ULL << square); }
inline void pop_bit(Bitboard& bitboard, const int square) { bitboard &= ~(1ULL << square); }

[[nodiscard]] constexpr int GetLsbIndex(Bitboard bitboard) {
    assert(bitboard);
    return std::countr_zero(bitboard);
}

constexpr int popLsb(Bitboard& bitboard) {
    assert(bitboard);
    int square = GetLsbIndex(bitboard);
    bitboard &= bitboard - 1;
    return square;
}

constexpr int CountBits(Bitboard bitboard) {
    return std::popcount(bitboard);
}
