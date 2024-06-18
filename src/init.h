#pragma once

#include "types.h"

struct Position;
struct ThreadData;

inline Bitboard PieceKeys[12][64];
inline Bitboard enpassant_keys[64];
inline Bitboard SideKey;
inline Bitboard CastleKeys[16];

void InitNewGame(ThreadData* td);

// init slider piece's attack tables
void InitAttackTables();

void InitAll();
