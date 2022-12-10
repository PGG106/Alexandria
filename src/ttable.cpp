// pvtable.c
#include "ttable.h"
#include "Board.h"
#include "assert.h"
#include "io.h"
#include "makemove.h"
#include "move.h"
#include "movegen.h"
#include "stdio.h"
#include "stdlib.h"
#include <cstring>
#include <iostream>

S_HASHTABLE HashTable[1];

void ClearHashTable(S_HASHTABLE* table) {
	S_HASHENTRY* tableEntry;

	for (tableEntry = table->pTable;
		tableEntry < table->pTable + table->numEntries; tableEntry++) {
		tableEntry->tt_key = 0ULL;
		tableEntry->move = NOMOVE;
		tableEntry->depth = 0;
		tableEntry->score = 0;
		tableEntry->flags = 0;
	}
}

void InitHashTable(S_HASHTABLE* table, uint64_t MB) {
	uint64_t HashSize = 0x100000 * MB;
	table->numEntries = (HashSize / sizeof(S_HASHENTRY));
	table->numEntries -= 2;
	if (table->pTable != NULL)
		free(table->pTable);
	table->pTable = (S_HASHENTRY*)malloc(table->numEntries * sizeof(S_HASHENTRY));
	ClearHashTable(table);

	std::cout << "HashTable init complete with " << table->numEntries << " entries" << std::endl;
}

bool ProbeHashEntry(S_Board* pos, int alpha, int beta, int depth,
	S_HASHENTRY* tte) {
	uint64_t index = Index(pos->posKey);

	std::memcpy(tte, &HashTable->pTable[index], sizeof(S_HASHENTRY));

	if (tte->score > ISMATE)
		tte->score -= pos->ply;
	else if (tte->score < -ISMATE)
		tte->score += pos->ply;

	return (HashTable->pTable[index].tt_key == (TTKey)pos->posKey);
}

void StoreHashEntry(S_Board* pos, const int move, int score, const int flags,
	const int depth, const bool pv) {
	uint64_t index = Index(pos->posKey);

	if (score > ISMATE)
		score -= pos->ply;
	else if (score < -ISMATE)
		score += pos->ply;

	// Replacement strategy taken from Stockfish
	//  Preserve any existing move for the same position
	if (move != NOMOVE || (uint16_t)pos->posKey != HashTable->pTable[index].tt_key)
		HashTable->pTable[index].move = move;

	// Overwrite less valuable entries (cheapest checks first)
	if (flags == HFEXACT ||
		(TTKey)pos->posKey != HashTable->pTable[index].tt_key ||
		depth + 7 + 2 * pv > HashTable->pTable[index].depth - 4)
	{
		HashTable->pTable[index].tt_key = (TTKey)pos->posKey;
		HashTable->pTable[index].flags = (uint8_t)flags;
		HashTable->pTable[index].score = (int16_t)score;
		HashTable->pTable[index].depth = (uint8_t)depth;
	}
}

int ProbePvMove(S_Board* pos) {
	uint64_t index = Index(pos->posKey);
	assert(index >= 0 && index <= HashTable->numEntries - 1);
	if (HashTable->pTable[index].tt_key == (TTKey)pos->posKey) {
		return HashTable->pTable[index].move;
	}
	return NOMOVE;
}

uint64_t Index(PosKey posKey) {
	return  ((uint32_t)posKey * (uint64_t)(HashTable->numEntries)) >> 32;

}

//prefetches the data in the given address in l1/2 cache in a non blocking way.
void prefetch(void* addr) {
#  if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch((char*)addr, _MM_HINT_T0);
#  else
	__builtin_prefetch(addr);
#  endif
}
