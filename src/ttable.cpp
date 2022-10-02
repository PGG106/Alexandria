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

int GetPvLine(const int depth, S_Board* pos) {

	assert(depth <= MAXDEPTH);

	int move = ProbePvMove(pos);
	int count = 0;

	while (move != NOMOVE && count < depth) {

		assert(count <= MAXDEPTH);

		if (MoveExists(pos, move)) {
			make_move(move, pos);
			pos->pvArray[count++] = move;
			move = ProbePvMove(pos);
		}

		else {
			break;
		}
	}

	while (pos->ply > 0) {
		Unmake_move(pos);
	}

	return count;
}

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

	int index = pos->posKey % HashTable->numEntries;

	std::memcpy(tte, &HashTable->pTable[index], sizeof(S_HASHENTRY));

	if (tte->score > ISMATE)
		tte->score -= pos->ply;
	else if (tte->score < -ISMATE)
		tte->score += pos->ply;

	return (HashTable->pTable[index].tt_key == (TTKey)pos->posKey);
}

void StoreHashEntry(S_Board* pos, const int move, int score, const int flags,
	const int depth, const bool pv) {

	int index = pos->posKey % HashTable->numEntries;

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

	int index = pos->posKey % HashTable->numEntries;
	assert(index >= 0 && index <= HashTable->numEntries - 1);
	if (HashTable->pTable[index].tt_key == (TTKey)pos->posKey) {
		return HashTable->pTable[index].move;
	}
	return NOMOVE;
}

//prefetches the data in the given address in l1/2 cache in a non blocking way.
void prefetch(void* addr) {

#  if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch((char*)addr, _MM_HINT_T0);
#  else
	__builtin_prefetch(addr);
#  endif
}
