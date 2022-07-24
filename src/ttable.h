#pragma once
#include "Board.h"
#include "stdlib.h"

int GetPvLine(const int depth, S_Board *pos);

void ClearHashTable(S_HASHTABLE *table);
void InitHashTable(S_HASHTABLE *table, int MB);

int ProbeHashEntry(S_Board *pos, int alpha, int beta, int depth,
                   S_HASHENTRY *tte);
void StoreHashEntry(S_Board *pos, const int move, int score, const int flags,
                    const int depth, const bool pv);
int ProbePvMove(S_Board *pos);
