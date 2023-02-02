#pragma once
#include "board.h"
#include <cstdint>

// get bishop attacks
Bitboard get_bishop_attacks(const int square, Bitboard occupancy);
// get rook attacks
Bitboard get_rook_attacks(const int square, Bitboard occupancy);

// get queen attacks
Bitboard get_queen_attacks(const int square, Bitboard occupancy);
