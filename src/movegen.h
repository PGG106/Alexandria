#pragma once

#include <cstdint>
#include "types.h"

struct Position;
struct MoveList;

enum MovegenType : uint8_t {
    MOVEGEN_NOISY = 0b01,
    MOVEGEN_QUIET = 0b10,
    MOVEGEN_ALL   = 0b11
};

// is the square given in input attacked by the current given side
[[nodiscard]] bool IsSquareAttacked(const Position* pos, const int square, const int side);

// function that adds a (not yet scored) move to a move list
void AddMove(const Move move, MoveList* list);

// function that adds an (already-scored) move to a move list
void AddMove(const Move move, const int score, MoveList* list);

// Check for move legality by generating the list of legal moves in a position and checking if that move is present
[[nodiscard]] bool MoveExists(Position* pos, const Move move);

void quietChecks(Position* pos, MoveList* movelist);

// Check for move pseudo-legality
[[nodiscard]] bool IsPseudoLegal(Position* pos, Move move);

// Check for move legality
[[nodiscard]] bool IsLegal(Position* pos, Move move);

// generate moves
void GenerateMoves(MoveList* move_list, Position* pos, MovegenType type);
