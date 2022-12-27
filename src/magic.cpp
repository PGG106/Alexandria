#include "magic.h"
#include "attack.h"
#include "random.h"
#include "stdio.h"
#include "string.h"

// get bishop attacks
Bitboard get_bishop_attacks(const int square, Bitboard occupancy) {
	// get bishop attacks assuming current board occupancy
	occupancy &= bishop_masks[square];
	occupancy *= bishop_magic_numbers[square];
	occupancy >>= 64 - bishop_relevant_bits[square];

	// return bishop attacks
	return bishop_attacks[square][occupancy];
}

// get rook attacks
Bitboard get_rook_attacks(const int square, Bitboard occupancy) {
	// get rook attacks assuming current board occupancy
	occupancy &= rook_masks[square];
	occupancy *= rook_magic_numbers[square];
	occupancy >>= 64 - rook_relevant_bits[square];

	// return rook attacks
	return rook_attacks[square][occupancy];
}

// get queen attacks
Bitboard get_queen_attacks(const int square, Bitboard occupancy) {
	// init result attacks bitboard
	Bitboard queen_attacks = 0ULL;

	// init bishop occupancies
	Bitboard bishop_occupancy = occupancy;

	// init rook occupancies
	Bitboard rook_occupancy = occupancy;

	// get bishop attacks assuming current board occupancy
	bishop_occupancy &= bishop_masks[square];
	bishop_occupancy *= bishop_magic_numbers[square];
	bishop_occupancy >>= 64 - bishop_relevant_bits[square];

	// get bishop attacks
	queen_attacks = bishop_attacks[square][bishop_occupancy];

	// get rook attacks assuming current board occupancy
	rook_occupancy &= rook_masks[square];
	rook_occupancy *= rook_magic_numbers[square];
	rook_occupancy >>= 64 - rook_relevant_bits[square];

	// get rook attacks
	queen_attacks |= rook_attacks[square][rook_occupancy];

	// return queen attacks
	return queen_attacks;
}
