#pragma once
#include "board.h"
#include "hashkey.h"
#include "init.h"
#include "io.h"
#include "makemove.h"
#include "movegen.h"

#define HASH_PCE(pce, sq) (pos->posKey ^= (PieceKeys[(pce)][(sq)]))

/// <summary>
/// Removes a piece from a square
/// </summary>
/// <param name="piece">the piece to be removed</param>
/// <param name="sq">the square the piece sits on</param>
/// <param name="pos">the current position</param>
void ClearPiece(const int piece, const int from, S_Board* pos);
/// <summary>
/// Adds a pice to a square
/// </summary>
/// <param name="piece">the piece to be added</param>
/// <param name="to">the square the piece will be added to</param>
/// <param name="pos">the current position</param>
void AddPiece(const int piece, const int to, S_Board* pos);

void MovePiece(const int piece, const int from, const int to, S_Board* pos);

void UpdateCastlingPerms(S_Board* pos, int source_square, int target_square);

void HashKey(S_Board* pos, ZobristKey key);

// make move on chess board
void make_move(const int move, S_Board* pos);
int make_move_light(const int move, S_Board* pos);
//Reverts the previously played move
int Unmake_move(const int move, S_Board* pos);
//makes a null move (a move that doesn't move any piece)
void MakeNullMove(S_Board* pos);
//Reverts the previously played null move
void TakeNullMove(S_Board* pos);
