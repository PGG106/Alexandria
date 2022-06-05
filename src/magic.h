#pragma once
#include "Board.h"

// init slider piece's attack tables
void init_sliders_attacks(int bishop);
// get bishop attacks
Bitboard get_bishop_attacks(int square, Bitboard occupancy);
// get rook attacks
Bitboard get_rook_attacks(int square, Bitboard occupancy);

// get queen attacks
Bitboard get_queen_attacks(int square, Bitboard occupancy);