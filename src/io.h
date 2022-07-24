#pragma once
#include "Board.h"
#include "move.h"
void print_bitboard(Bitboard bitboard);

// print board
void print_board(S_Board *pos);

// print attacked squares
void print_attacked_squares(int side);

// print move (for UCI purposes)
void print_move(int move);

// print move list
void PrintMoveList(S_MOVELIST *move_list);

char *FormatMove(const int move);

int ParseMove(char *ptrChar, S_Board *pos);