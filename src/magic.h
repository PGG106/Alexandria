#pragma once

#include "attack.h"
#include "types.h"

// get bishop attacks
[[nodiscard]] inline Bitboard GetBishopAttacks(const int square, Bitboard occupancy) {
    // get bishop attacks assuming current board occupancy
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - bishop_relevant_bits;

    // return bishop attacks
    return bishop_attacks[square][occupancy];
}

// get rook attacks
[[nodiscard]] inline Bitboard GetRookAttacks(const int square, Bitboard occupancy) {
    // get rook attacks assuming current board occupancy
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - rook_relevant_bits;

    // return rook attacks
    return rook_attacks[square][occupancy];
}

// get queen attacks
[[nodiscard]] inline Bitboard GetQueenAttacks(const int square, Bitboard occupancy) {
    return GetBishopAttacks(square, occupancy) | GetRookAttacks(square, occupancy);
}
