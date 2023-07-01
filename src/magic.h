#pragma once

#include "types.h"

// get bishop attacks
[[nodiscard]] Bitboard GetBishopAttacks(const int square, Bitboard occupancy);

// get rook attacks
[[nodiscard]] Bitboard GetRookAttacks(const int square, Bitboard occupancy);

// get queen attacks
[[nodiscard]] Bitboard GetQueenAttacks(const int square, Bitboard occupancy);
