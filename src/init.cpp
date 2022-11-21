#include "Board.h"
#include "PieceData.h"
#include "attack.h"
#include "io.h"
#include "magic.h"
#include "random.h"
#include "stdint.h"
#include <cmath>

Bitboard PieceKeys[12][64];
Bitboard enpassant_keys[64];
Bitboard SideKey;
Bitboard CastleKeys[16];

Bitboard FileBBMask[8];
Bitboard RankBBMask[8];

// pawn attacks table [side][square]
Bitboard pawn_attacks[2][64];

// knight attacks table [square]
Bitboard knight_attacks[64];

// king attacks table [square]
Bitboard king_attacks[64];

// bishop attack masks
Bitboard bishop_masks[64];

// rook attack masks
Bitboard rook_masks[64];

// rook attack masks Bitboard rook_masks[64];

// bishop attacks table [square][pos->occupancies]
Bitboard bishop_attacks[64][512];

// rook attacks rable [square][pos->occupancies]
Bitboard rook_attacks[64][4096];

Bitboard SQUARES_BETWEEN_BB[64][64];

int reductions[MAXDEPTH];

//Initialize the Zobrist keys
void initHashKeys() {
	int Typeindex = 0;
	int Numberindex = 0;
	for (Typeindex = WP; Typeindex <= BK; ++Typeindex) {
		for (Numberindex = 0; Numberindex < 64; ++Numberindex) {
			PieceKeys[Typeindex][Numberindex] = get_random_Bitboard_number();
		}
	}
	// loop over board squares
	for (int square = 0; square < Board_sq_num; square++)
		// init random enpassant keys
		enpassant_keys[square] = get_random_Bitboard_number();

	// loop over castling keys
	for (int index = 0; index < 16; index++)
		// init castling keys
		CastleKeys[index] = get_random_Bitboard_number();

	// init random side key
	SideKey = get_random_Bitboard_number();
}

// init leaper pieces attacks
void init_leapers_attacks() {
	// loop over 64 board squares
	for (int square = 0; square < Board_sq_num; square++) {
		// init pawn attacks
		pawn_attacks[WHITE][square] = mask_pawn_attacks(WHITE, square);
		pawn_attacks[BLACK][square] = mask_pawn_attacks(BLACK, square);

		// init knight attacks
		knight_attacks[square] = mask_knight_attacks(square);

		// init king attacks
		king_attacks[square] = mask_king_attacks(square);
	}
}

// init slider piece's attack tables
void init_sliders_attacks(int bishop) {
	// loop over 64 board squares
	for (int square = 0; square < Board_sq_num; square++) {
		// init bishop & rook masks
		bishop_masks[square] = mask_bishop_attacks(square);
		rook_masks[square] = mask_rook_attacks(square);

		// init current mask
		Bitboard attack_mask = bishop ? bishop_masks[square] : rook_masks[square];

		// init relevant occupancy bit count
		int relevant_bits_count = count_bits(attack_mask);

		// init occupancy indicies
		int occupancy_indicies = (1 << relevant_bits_count);

		// loop over occupancy indicies
		for (int index = 0; index < occupancy_indicies; index++) {
			// bishop
			if (bishop) {
				// init current occupancy variation
				Bitboard occupancy =
					set_occupancy(index, relevant_bits_count, attack_mask);

				// init magic index
				int magic_index = (occupancy * bishop_magic_numbers[square]) >>
					(64 - bishop_relevant_bits[square]);

				// init bishop attacks
				bishop_attacks[square][magic_index] =
					bishop_attacks_on_the_fly(square, occupancy);
			}

			// rook
			else {
				// init current occupancy variation
				Bitboard occupancy =
					set_occupancy(index, relevant_bits_count, attack_mask);

				// init magic index
				int magic_index = (occupancy * rook_magic_numbers[square]) >>
					(64 - rook_relevant_bits[square]);

				// init rook attacks
				rook_attacks[square][magic_index] =
					rook_attacks_on_the_fly(square, occupancy);
			}
		}
	}
}

void initializeLookupTables() {
	// initialize squares between table
	Bitboard sqs;
	for (int sq1 = 0; sq1 <= 63; ++sq1) {
		for (int sq2 = 0; sq2 <= 63; ++sq2) {
			sqs = (1ULL << sq1) | (1ULL << sq2);
			if (get_file[sq1] == get_file[sq2] || (get_rank[sq1] == get_rank[sq2]))
				SQUARES_BETWEEN_BB[sq1][sq2] =
				get_rook_attacks(sq1, sqs) & get_rook_attacks(sq2, sqs);
			else if ((get_diagonal[sq1] == get_diagonal[sq2]) ||
				(get_antidiagonal(sq1) == get_antidiagonal(sq2)))
				SQUARES_BETWEEN_BB[sq1][sq2] =
				get_bishop_attacks(sq1, sqs) & get_bishop_attacks(sq2, sqs);
		}
	}
}

// BIG THANKS TO DISSERVIN FOR LETTING ME BORROW THIS CODE
Bitboard DoCheckmask(S_Board* pos, int color, int sq) {
	Bitboard Occ = pos->occupancies[2];
	Bitboard checks = 0ULL;
	Bitboard pawn_mask =
		pos->bitboards[(color ^ 1) * 6] & pawn_attacks[color][sq];
	Bitboard knight_mask =
		pos->bitboards[(color ^ 1) * 6 + 1] & knight_attacks[sq];
	Bitboard bishop_mask = (pos->bitboards[(color ^ 1) * 6 + 2] |
		pos->bitboards[(color ^ 1) * 6 + 4]) &
		get_bishop_attacks(sq, Occ) & ~pos->occupancies[color];
	Bitboard rook_mask = (pos->bitboards[(color ^ 1) * 6 + 3] |
		pos->bitboards[(color ^ 1) * 6 + 4]) &
		get_rook_attacks(sq, Occ) & ~pos->occupancies[color];
	pos->checks = 0;
	if (pawn_mask) {
		checks |= pawn_mask;
		pos->checks++;
	}
	if (knight_mask) {
		checks |= knight_mask;
		pos->checks++;
	}
	if (bishop_mask) {
		if (count_bits(bishop_mask) > 1)
			pos->checks++;

		int8_t index = get_ls1b_index(bishop_mask);
		checks |= SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
		pos->checks++;
	}
	if (rook_mask) {
		if (count_bits(rook_mask) > 1)
			pos->checks++;

		int8_t index = get_ls1b_index(rook_mask);
		checks |= SQUARES_BETWEEN_BB[sq][index] | (1ULL << index);
		pos->checks++;
	}
	return checks;
}

void DoPinMask(S_Board* pos, int color, int sq) {
	Bitboard them = Occupancy(pos, color ^ 1);
	Bitboard bishop_mask = (pos->bitboards[(color ^ 1) * 6 + 2] |
		pos->bitboards[(color ^ 1) * 6 + 4]) &
		get_bishop_attacks(sq, them);
	Bitboard rook_mask = (pos->bitboards[(color ^ 1) * 6 + 3] |
		pos->bitboards[0 + (color ^ 1) * 6 + 4]) &
		get_rook_attacks(sq, them);
	Bitboard rook_pin = 0ULL;
	Bitboard bishop_pin = 0ULL;
	pos->pinD = 0ULL;
	pos->pinHV = 0ULL;

	while (rook_mask) {
		int index = get_ls1b_index(rook_mask);
		Bitboard possible_pin = (SQUARES_BETWEEN_BB[sq][index] | (1ULL << index));
		if (count_bits(possible_pin & pos->occupancies[color]) == 1)
			rook_pin |= possible_pin;
		pop_bit(rook_mask, index);
	}
	while (bishop_mask) {
		int index = get_ls1b_index(bishop_mask);
		Bitboard possible_pin = (SQUARES_BETWEEN_BB[sq][index] | (1ULL << index));
		if (count_bits(possible_pin & pos->occupancies[color]) == 1)
			bishop_pin |= possible_pin;
		pop_bit(bishop_mask, index);
	}
	pos->pinHV = rook_pin;
	pos->pinD = bishop_pin;
}

//PreCalculate the logarithms used in the reduction calculation
void InitReductions() {
	for (int i = 0; i < MAXDEPTH; i++) {
		reductions[i] = log(i);
	}
}

void init_all() {
	// init leaper pieces attacks
	init_leapers_attacks();

	// init slider pieces attacks
	init_sliders_attacks(bishop);
	init_sliders_attacks(rook);
	initializeLookupTables();
	initHashKeys();
	InitReductions();
	nnue.init("nn.net");
}
