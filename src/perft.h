#pragma once

struct Position;

// perft driver
void PerftDriver(int depth, Position* pos);

// perft test
unsigned long long PerftTest(int depth, Position* pos);
