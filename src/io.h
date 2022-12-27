#pragma once
#include "Board.h"
#include "move.h"
void print_bitboard(const Bitboard bitboard);

// print board
void print_board(const S_Board* pos);

// print attacked squares
void print_attacked_squares(const S_Board* pos, const int side);

// print move (for UCI purposes)
void print_move(const int move);

// print move list
void PrintMoveList(const S_MOVELIST* move_list);

char* FormatMove(const int move);
