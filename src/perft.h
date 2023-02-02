#pragma once
#include "board.h"
// perft driver
void perft_driver(int depth, S_Board *pos);
// perft test
unsigned long long perft_test(int depth, S_Board *pos);