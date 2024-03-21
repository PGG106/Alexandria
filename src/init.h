#pragma once

#include "types.h"

struct Position;
struct S_ThreadData;

extern Bitboard PieceKeys[12][64];
extern Bitboard enpassant_keys[64];
extern Bitboard SideKey;
extern Bitboard CastleKeys[16];

void InitNewGame(S_ThreadData* td);

void InitLeapersAttacks();

// init slider piece's attack tables
void InitSlidersAttacks();

void InitAll();
