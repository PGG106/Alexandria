#pragma once

#include "types.h"

struct Position;

template <bool UPDATE>
void ClearPiece(const int piece, const int from, Position* pos);

template <bool UPDATE>
void AddPiece(const int piece, const int to, Position* pos);

void UpdateCastlingPerms(Position* pos, int source_square, int target_square);

void HashKey(Position* pos, ZobristKey key);

template <bool UPDATE>
void MakeMove(const Move move, Position* pos);
// Reverts the previously played move
void UnmakeMove(const Move move, Position* pos);
// makes a null move (a move that doesn't move any piece)
void MakeNullMove(Position* pos);
// Reverts the previously played null move
void TakeNullMove(Position* pos);
