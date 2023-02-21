#pragma once
#include "types.h"
#include "board.h"

// not A file constant
extern const Bitboard not_a_file;

// not H file constant
extern const Bitboard not_h_file;

// not HG file constant
extern const Bitboard not_hg_file;

// not AB file constant
extern const Bitboard not_ab_file;

// bishop relevant occupancy bit count for every square on board
extern const int bishop_relevant_bits[64];

// rook relevant occupancy bit count for every square on board
extern const int rook_relevant_bits[64];

// rook magic numbers
extern Bitboard rook_magic_numbers[64];

// bishop magic numbers
extern Bitboard bishop_magic_numbers[64];

// generate pawn attacks
Bitboard MaskPawnAttacks(int side, int square);

// generate knight attacks
Bitboard MaskKnightAttacks(int square);
// generate king attacks
Bitboard MaskKingAttacks(int square);

// mask bishop attacks
Bitboard MaskBishopAttacks(int square);

// mask rook attacks
Bitboard MaskRookAttacks(int square);
// generate bishop attacks on the fly
Bitboard BishopAttacksOnTheFly(int square, Bitboard block);

// generate rook attacks on the fly
Bitboard RookAttacksOnTheFly(int square, Bitboard block);

// pawn attacks table [side][square]
extern Bitboard pawn_attacks[2][64];

// knight attacks table [square]
extern Bitboard knight_attacks[64];

// king attacks table [square]
extern Bitboard king_attacks[64];

// bishop attack masks
extern Bitboard bishop_masks[64];

// rook attack masks
extern Bitboard rook_masks[64];

// bishop attacks table [square][occupancies]
extern Bitboard bishop_attacks[64][512];

// rook attacks rable [square][occupancies]
extern Bitboard rook_attacks[64][4096];

// set occupancies
Bitboard SetOccupancy(int index, int bits_in_mask, Bitboard attack_mask);

