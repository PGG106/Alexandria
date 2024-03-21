#pragma once

#include "types.h"

struct Position;

void ClearPiece(const int piece, const int from, Position* pos);

void AddPiece(const int piece, const int to, Position* pos);

void MovePiece(const int piece, const int from, const int to, Position* pos);

void UpdateCastlingPerms(Position* pos, int source_square, int target_square);

void HashKey(Position* pos, ZobristKey key);

// make move on chess board
void MakeMove(const int move, Position* pos);
// Reverts the previously played move
void UnmakeMove(const int move, Position* pos);
// makes a null move (a move that doesn't move any piece)
void MakeNullMove(Position* pos);
// Reverts the previously played null move
void TakeNullMove(Position* pos);
// Makes a move without setting up the variables to ever reverse it, should only be used on moves that come directly from uci
void MakeUCIMove(const int move, Position* pos);
