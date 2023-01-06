#pragma once
#include "Board.h"
#include "hashkey.h"
#include "init.h"
#include "io.h"
#include "makemove.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "stdio.h"
#include "string.h"
#include "uci.h"

#define HASH_PCE(pce, sq) (pos->posKey ^= (PieceKeys[(pce)][(sq)]))
#define HASH_CA (pos->posKey ^= (CastleKeys[(pos->castleperm)]))
#define HASH_SIDE (pos->posKey ^= (SideKey))
#define HASH_EP (pos->posKey ^= (enpassant_keys[(pos->enPas)]))

/// <summary>
/// Removes a piece from a square
/// </summary>
/// <param name="piece">the piece to be removed</param>
/// <param name="sq">the square the piece sits on</param>
/// <param name="pos">the current position</param>
void ClearPiece(const int piece, const int sq, S_Board* pos);
/// <summary>
/// Adds a pice to a square
/// </summary>
/// <param name="piece">the piece to be added</param>
/// <param name="to">the square the piece will be added to</param>
/// <param name="pos">the current position</param>
void AddPiece(const int piece, const int to, S_Board* pos);

void MovePiece(const int piece, const int from, const int to, S_Board* pos);

// make move on chess board
int make_move(const int move, S_Board* pos);
int make_move_light(const int move, S_Board* pos);
//Reverts the previously played move
int Unmake_move(const int move, S_Board* pos);
//makes a null move (a move that doesn't move any piece)
void MakeNullMove(S_Board* pos);
//Reverts the previously played null move
void TakeNullMove(S_Board* pos);

PosKey KeyAfterMove(const S_Board* pos, const PosKey OldKey, const  int move);