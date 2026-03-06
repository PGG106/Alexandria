#pragma once
#include "types.h"

struct Position;

// perft driver
void PerftDriver(int depth, Position* pos, std::vector<ZobristKey>& keyHistory);

// perft test
unsigned long long PerftTest(int depth, Position* pos, std::vector<ZobristKey>& keyHistory);
