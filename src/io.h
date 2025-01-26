#pragma once

#include "types.h"

struct Position;
struct MoveList;
struct ThreadData;
struct UciOptions;

void PrintBitboard(const Bitboard bitboard);

// print board
void PrintBoard(const Position* pos);

// print attacked squares
void PrintAttackedSquares(const Position* pos, const int side);

// print move (for UCI purposes)
void PrintMove(const Move move);

// print move list
void PrintMoveList(const MoveList* list);

[[nodiscard]] char* FormatMove(const Move move);

void PrintUciOutput(const int score, const int depth, const UciOptions* options);
