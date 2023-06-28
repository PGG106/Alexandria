#pragma once

#include "types.h"

struct S_Board;
struct S_ThreadData;

extern Bitboard PieceKeys[12][64];
extern Bitboard enpassant_keys[64];
extern Bitboard SideKey;
extern Bitboard CastleKeys[16];

void InitNewGame(S_ThreadData* td);

void InitLeapersAttacks();

// init slider piece's attack tables
void InitSlidersAttacks(const int bishop);

void InitAll();

Bitboard DoCheckmask(S_Board* pos, int color, int sq);

void DoPinMask(S_Board* pos, int color, int sq);
