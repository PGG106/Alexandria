#pragma once
#include "Board.h"
#include "PieceData.h"
#include "move.h"
#include "movegen.h"
#include "stdio.h"

#define take_back()                                                            \
  memcpy(pos->bitboards, pos->bitboards_copy, 96);                             \
  memcpy(occupancies, occupancies_copy, 24);                                   \
  side = side_copy, enpassant = enpassant_copy, castle = castle_copy;

// perft driver
void perft_driver(int depth, S_Board *pos);
// perft test
unsigned long long perft_test(int depth, S_Board *pos);