#pragma once
#include "Board.h"

extern Bitboard PieceKeys[12][64];
extern Bitboard enpassant_keys[64];
extern Bitboard SideKey;
extern Bitboard CastleKeys[16];

// double pawns penalty
extern const int double_pawn_penalty;

// isolated pawn penalty
extern const int isolated_pawn_penalty;

// passed pawn bonus
extern const int passed_pawn_bonus[8];

// semi open file score
extern const int semi_open_file_score;

// open file score
extern const int open_file_score;

// king's shield bonus
extern const int king_shield_bonus;

void init_leapers_attacks();

// init slider piece's attack tables
void init_sliders_attacks(int bishop);

void InitPolyBook();

void init_all();

Bitboard DoCheckmask(S_Board *pos, int color, int sq);

void DoPinMask(S_Board *pos, int color, int sq);
