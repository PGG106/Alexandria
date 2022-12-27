#pragma once
#include "Board.h"
#include "move.h"

// is the square given in input attacked by the current given side
bool is_square_attacked(const S_Board* pos, const int square, const int side);

//Check for move legality by generating the list of legal moves in a position and checking if that move is present
int MoveExists(const S_Board* pos, const int move);

// generate all moves
void generate_moves(S_MOVELIST* move_list, S_Board* pos);

// generate all moves
void generate_captures(S_MOVELIST* move_list, S_Board* pos);