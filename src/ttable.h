#pragma once
#include "stdlib.h"
#include "Board.h"


int GetPvLine(const int depth, S_Board* pos);


void ClearHashTable(S_HASHTABLE* table);
void InitHashTable(S_HASHTABLE* table);

int ProbeHashEntry(S_Board* pos, int* move, int* score, int alpha, int beta, int depth);
void StoreHashEntry(S_Board* pos, const int move, int score, const int flags, const int depth);
int ProbePvMove( S_Board* pos);

