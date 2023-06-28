#pragma once

#include "board.h"

// perft driver
void PerftDriver(int depth, S_Board *pos);
// perft test
unsigned long long PerftTest(int depth, S_Board *pos);
