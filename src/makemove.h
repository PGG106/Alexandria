#pragma once

#include "types.h"

struct Position;

void ClearPiece(const int piece, const int from, Position* pos);

void AddPiece(const int piece, const int to, Position* pos);

void UpdateCastlingPerms(Position* pos, int source_square, int target_square);

template <bool UPDATE>
void MakeMove(const Move move, Position* pos);
// Reverts the previously played move
void UnmakeMove(Position* pos);
// makes a null move (a move that doesn't move any piece)
void MakeNullMove(Position* pos);
// Reverts the previously played null move
void TakeNullMove(Position* pos);
