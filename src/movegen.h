#pragma once

struct S_Board;
struct S_MOVELIST;

// is the square given in input attacked by the current given side
bool IsSquareAttacked(const S_Board* pos, const int square, const int side);

//Check for move legality by generating the list of legal moves in a position and checking if that move is present
int MoveExists(const S_Board* pos, const int move);

// generate all moves
void GenerateMoves(S_MOVELIST* move_list, S_Board* pos);

// generate all moves
void GenerateCaptures(S_MOVELIST* move_list, S_Board* pos);
