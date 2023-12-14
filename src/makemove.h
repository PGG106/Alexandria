#pragma once

#include "types.h"

struct S_Board;

void ClearPiece(const int piece, const int from, S_Board* pos);

void AddPiece(const int piece, const int to, S_Board* pos);

void MovePiece(const int piece, const int from, const int to, S_Board* pos);

void UpdateCastlingPerms(S_Board* pos, int source_square, int target_square);

void HashKey(S_Board* pos, ZobristKey key);

// make move on chess board
void MakeMove(const int move, S_Board* pos);
// Reverts the previously played move
void UnmakeMove(const int move, S_Board* pos);
// makes a null move (a move that doesn't move any piece)
void MakeNullMove(S_Board* pos);
// Reverts the previously played null move
void TakeNullMove(S_Board* pos);
