#pragma once
#include "Board.h"

// pawn positional score
extern const int pawn_score[64];

// knight positional score
extern const int knight_score[64];

// bishop positional score
extern const int bishop_score[64];

// rook positional score
extern const int rook_score[64];

// king positional score
extern const int king_score[64];
// mirror positional score tables for opposite side
extern const int mirror_score[128];
int MaterialDraw(const S_Board *pos);
// position evaluation
int EvalPosition(const S_Board *pos);
