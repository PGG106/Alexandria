#pragma once

#include "types.h"

struct Position;
struct ThreadData;

inline ZobristKey PieceKeys[12][64];
inline ZobristKey enpassant_keys[64];
inline ZobristKey SideKey;
inline ZobristKey CastleKeys[16];
inline ZobristKey MoveRuleKeys[101];

void InitNewGame(ThreadData* td);

// init slider piece's attack tables
void InitAttackTables();

void InitAll();
// has to be exposed for tuning refreshes
void InitReductions();
