#pragma once
#include "Board.h"
#include "move.h"

// is square current given attacked by the current given side
bool is_square_attacked(const S_Board *pos, int square, int side);
int MoveExists(S_Board *pos, const int move);

// generate all moves
void generate_moves(S_MOVELIST *move_list, S_Board *pos);
void generate_captures(S_MOVELIST *move_list, S_Board *pos);