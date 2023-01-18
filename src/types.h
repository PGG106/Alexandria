#pragma once
#include "stdint.h"

// define bitboard data type
typedef uint64_t Bitboard;
// define poskey data type
typedef uint16_t TTKey;
// define poskey data type
typedef uint64_t PosKey;

// board squares
enum {
	a8, b8, c8, d8, e8, f8, g8, h8,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};

enum {
	FILE_A,
	FILE_B,
	FILE_C,
	FILE_D,
	FILE_E,
	FILE_F,
	FILE_G,
	FILE_H,
	FILE_NONE
};
enum {
	RANK_1,
	RANK_2,
	RANK_3,
	RANK_4,
	RANK_5,
	RANK_6,
	RANK_7,
	RANK_8,
	RANK_NONE
};

// encode pieces
enum {
	WP, WN, WB, WR, WQ, WK,
	BP, BN, BB, BR, BQ, BK,
	EMPTY = 14
};

// sides to move (colors)
enum {
	WHITE,
	BLACK,
	BOTH
};

// bishop and rook
enum {
	rook,
	bishop
};

enum {
	WKCA = 1,
	WQCA = 2,
	BKCA = 4,
	BQCA = 8
};

enum {
	FALSE,
	TRUE
};

enum {
	HFNONE,
	HFALPHA,
	HFBETA,
	HFEXACT
};

// game phase scores
const int opening_phase_score = 6192;
const int endgame_phase_score = 518;

// game phases
enum {
	opening,
	endgame,
	middlegame
};

// piece types
enum {
	PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
};

//Contains the material Values of the pieces
constexpr int PieceValue[15] = { 100, 300, 300, 450, 900, 0,
					  100, 300, 300, 450, 900, 0,0,0,0 };

enum {
	goodCaptureScore= 900000000,
	killerMoveScore0 = 800000000,
	killerMoveScore1 = 700000000
};
