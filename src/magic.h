#pragma once
#include "board.h"
#include <cstdint>

// get bishop attacks
Bitboard GetBishopAttacks(const int square, Bitboard occupancy);
// get rook attacks
Bitboard GetRookAttacks(const int square, Bitboard occupancy);

// get queen attacks
Bitboard GetQueenAttacks(const int square, Bitboard occupancy);
