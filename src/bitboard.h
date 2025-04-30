#pragma once
#include <bit>
#include <cassert>
#include "types.h"

// set/get/pop bit macros
inline void set_bit(Bitboard& bitboard, const int square) { bitboard |= (1ULL << square); }
[[nodiscard]] inline bool get_bit(const Bitboard bitboard, const int square) { return bitboard & (1ULL << square);}
inline void pop_bit(Bitboard& bitboard, const int square) { bitboard &= ~(1ULL << square); }

[[nodiscard]] inline int GetLsbIndex(Bitboard bitboard) {
    assert(bitboard);
    return std::countr_zero(bitboard);
}

inline int popLsb(Bitboard& bitboard) {
    assert(bitboard);
    int square = GetLsbIndex(bitboard);
    bitboard &= bitboard - 1;
    return square;
}

inline int CountBits(Bitboard bitboard) {
    return std::popcount(bitboard);
}
