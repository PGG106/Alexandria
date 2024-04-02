#include <cstdint>
#pragma once

struct Position;
struct MoveList;

enum MovegenType : uint8_t {
    MOVEGEN_NOISY = 0b01,
    MOVEGEN_QUIET = 0b10,
    MOVEGEN_ALL   = 0b11
};

// is the square given in input attacked by the current given side
[[nodiscard]] bool IsSquareAttacked(const Position* pos, const int square, const int side);

// function that adds a move to the move list
void AddMove(int move, MoveList* list);

// Check for move legality by generating the list of legal moves in a position and checking if that move is present
[[nodiscard]] bool MoveExists(Position* pos, const int move);

// Check for move pseudo-legality
[[nodiscard]] bool IsPseudoLegal(Position* pos, int move);

// Check for move legality
[[nodiscard]] bool IsLegal(Position* pos, int move);

// generate moves
void GenerateMoves(MoveList* move_list, Position* pos, MovegenType type);
