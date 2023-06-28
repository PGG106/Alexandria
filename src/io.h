#pragma once

#include "types.h"

struct S_Board;
struct S_MOVELIST;
struct S_ThreadData;
struct S_UciOptions;

void PrintBitboard(const Bitboard bitboard);

// print board
void PrintBoard(const S_Board* pos);

// print attacked squares
void PrintAttackedSquares(const S_Board* pos, const int side);

// print move (for UCI purposes)
void PrintMove(const int move);

// print move list
void PrintMoveList(const S_MOVELIST* move_list);

[[nodiscard]] char* FormatMove(const int move);

void PrintUciOutput(const int score, const int depth, const S_ThreadData* td, const S_UciOptions* options);
