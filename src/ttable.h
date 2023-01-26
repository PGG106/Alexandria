#pragma once
#include "Board.h"
#include "stdlib.h"
#include <memory>

PACK(struct S_HashEntry {
	int32_t move = NOMOVE;
	int16_t score = 0;
	int16_t eval = 0;
	TTKey tt_key = 0;
	uint8_t depth = 0;
	uint8_t flags = HFNONE;
});

struct S_HashTable {
	std::vector<S_HashEntry> pTable;
};

extern S_HashTable HashTable[1];

void ClearHashTable(S_HashTable* table);
//Initialize an Hashtable of size MB
void InitHashTable(S_HashTable* table, uint64_t MB);

bool ProbeHashEntry(const S_Board* pos, S_HashEntry* tte);
/// <summary>
/// Store a move in the TT
/// </summary>
/// <param name="pos">The position from which the move can the played</param>
/// <param name="move">The move we have to store</param>
/// <param name="score">The search score of the move</param>
/// <param name="flags">a flag that represents if the score is exact or an alpha/beta bound</param>
/// <param name="depth"><The depth we've searched the move at/param>
/// <param name="pv">if the node we've found the move in was or not a pv node</param>
void StoreHashEntry(const S_Board* pos, const int move, int score, int16_t eval, const int flags,
	const int depth, const bool pv);
uint64_t Index(const PosKey posKey);
void TTPrefetch(const PosKey posKey);