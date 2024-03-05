#pragma once

struct S_Board;
struct S_MOVELIST;

// is the square given in input attacked by the current given side
[[nodiscard]] bool IsSquareAttacked(const S_Board* pos, const int square, const int side);

// function that adds a move to the move list
void AddMove(int move, S_MOVELIST* list);

// Check for move legality by generating the list of legal moves in a position and checking if that move is present
[[nodiscard]] bool MoveExists(S_Board* pos, const int move);

// Check for move pseudo-legality
[[nodiscard]] bool IsPseudoLegal(S_Board* pos, int move);

// Check for move legality
[[nodiscard]] bool IsLegal(S_Board* pos, int move);

// generate all moves
void GenerateMoves(S_MOVELIST* move_list, S_Board* pos);

// generate all captures
void GenerateCaptures(S_MOVELIST* move_list, S_Board* pos);
