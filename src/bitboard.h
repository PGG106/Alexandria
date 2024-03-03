#pragma once
#include <bit>
#include <cassert>
#include "types.h"

// set/get/pop bit macros
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))

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
