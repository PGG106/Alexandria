#pragma once
#include "Board.h"
#include "stdlib.h"

PACK(typedef struct HASHENTRY {
	int32_t move = NOMOVE;
	int16_t score = 0;
	TTKey tt_key = 0;
	uint8_t depth = 0;
	uint8_t flags = HFNONE;
})
S_HASHENTRY;

typedef struct HASHTABLE {
	S_HASHENTRY* pTable;
	uint64_t numEntries = 0;
} S_HASHTABLE;

extern S_HASHTABLE HashTable[1];

void ClearHashTable(S_HASHTABLE* table);
//Initialize an Hashtable of size MB
void InitHashTable(S_HASHTABLE* table, uint64_t MB);

bool ProbeHashEntry(S_Board* pos, int alpha, int beta, int depth,
	S_HASHENTRY* tte);
/// <summary>
/// Store a move in the TT
/// </summary>
/// <param name="pos">The position from which the move can the played</param>
/// <param name="move">The move we have to store</param>
/// <param name="score">The search score of the move</param>
/// <param name="flags">a flag that represents if the score is exact or an alpha/beta bound</param>
/// <param name="depth"><The depth we've searched the move at/param>
/// <param name="pv">if the node we've found the move in was or not a pv node</param>
void StoreHashEntry(S_Board* pos, const int move, int score, const int flags,
	const int depth, const bool pv);
int ProbePvMove(S_Board* pos);
uint64_t Index(PosKey posKey);
void prefetch(void* addr);