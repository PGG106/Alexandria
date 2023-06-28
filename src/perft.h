#pragma once

struct S_Board;

// perft driver
void PerftDriver(int depth, S_Board* pos);

// perft test
unsigned long long PerftTest(int depth, S_Board* pos);
