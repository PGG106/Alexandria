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

void ClearPiece(const int piece, const int sq, S_Board* pos);

void AddPiece(const int piece, const int to, S_Board* pos);

void MovePiece(const int piece, const int from, const int to, S_Board* pos);

// make move on chess board
int make_move(int move, S_Board* pos);

int Unmake_move(S_Board* pos);

void MakeNullMove(S_Board* pos);

void TakeNullMove(S_Board* pos);