#pragma once
#include "Board.h"

extern Bitboard PieceKeys[12][64];
extern Bitboard enpassant_keys[64];
extern Bitboard SideKey;
extern Bitboard CastleKeys[16];

void init_new_game(S_Board* pos, S_Stack* ss, S_SearchINFO* info);

void init_leapers_attacks();

// init slider piece's attack tables
void init_sliders_attacks(const int bishop);

void init_all();

Bitboard DoCheckmask(S_Board* pos, int color, int sq);

void DoPinMask(S_Board* pos, int color, int sq);
