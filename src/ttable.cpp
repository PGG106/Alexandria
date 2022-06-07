// pvtable.c
#include "stdlib.h"
#include "stdio.h"
#include "board.h"
#include "movegen.h"
#include "move.h"
#include "io.h"
#include "ttable.h"
#include "assert.h"
#include "makemove.h"

const int HashSize = 0x100000 * 80;

S_HASHTABLE HashTable[1];

int GetPvLine(const int depth, S_Board* pos) {

	assert(depth < MAXDEPTH);

	int move = ProbePvMove(pos);
	int count = 0;

	while (move != NOMOVE && count < depth) {

		assert(count < MAXDEPTH);

		if (MoveExists(pos, move)) {
			make_move(move, all_moves, pos);
			pos->pvArray[count++] = move;
		}
		else {
			break;
		}
		move = ProbePvMove(pos);
	}

	while (pos->ply > 0) {
		Unmake_move(pos);
	}

	return count;

}



void ClearHashTable(S_HASHTABLE* table) {

	S_HASHENTRY* tableEntry;

	for (tableEntry = table->pTable; tableEntry < table->pTable + table->numEntries; tableEntry++) {
		tableEntry->posKey = 0ULL;
		tableEntry->move = NOMOVE;
		tableEntry->depth = 0;
		tableEntry->score = 0;
		tableEntry->flags = 0;
	}
	table->newWrite = 0;
}

void InitHashTable(S_HASHTABLE* table) {

	table->numEntries = HashSize / sizeof(S_HASHENTRY);
	table->numEntries -= 2;
	if (table->pTable != NULL) free(table->pTable);
	table->pTable = (S_HASHENTRY*)malloc(table->numEntries * sizeof(S_HASHENTRY));
	ClearHashTable(table);
	printf("HashTable init complete with %d entries\n", table->numEntries);

}

int ProbeHashEntry(S_Board* pos, int* move, int* score, int alpha, int beta, int depth) {

	int index = pos->posKey % HashTable->numEntries;

	assert(index >= 0 && index <= HashTable->numEntries - 1);
	assert(depth >= 1 && depth < MAXDEPTH);
	assert(alpha < beta);
	assert(alpha >= -MAXSCORE && alpha <= MAXSCORE);
	assert(beta >= -MAXSCORE && beta <= MAXSCORE);
	assert(pos->ply >= 0 && pos->ply < MAXDEPTH);

	if (HashTable->pTable[index].posKey == pos->posKey) {
		if (MoveExists(pos, HashTable->pTable[index].move)) {
			*move = HashTable->pTable[index].move;
			if (HashTable->pTable[index].depth >= depth) {
				HashTable->hit++;

				assert(HashTable->pTable[index].depth >= 1 && HashTable->pTable[index].depth < MAXDEPTH);

				*score = HashTable->pTable[index].score;
				if (*score > ISMATE) *score -= pos->ply;
				else if (*score < -ISMATE) *score += pos->ply;

				switch (HashTable->pTable[index].flags) {


				case HFALPHA: if (*score <= alpha) {
					*score = alpha;
					return TRUE;
				}
							break;
				case HFBETA: if (*score >= beta) {
					*score = beta;
					return TRUE;
				}
						   break;
				case HFEXACT:
					return TRUE;
					break;
				default: assert(FALSE); break;
				}
			}
		}
	}


	return FALSE;
}

void StoreHashEntry(S_Board* pos, const int move, int score, const int flags, const int depth) {

	int index = pos->posKey % HashTable->numEntries;

	assert(index >= 0 && index <= HashTable->numEntries - 1);
	assert(depth >= 1 && depth < MAXDEPTH);
	assert(score >= -MAXSCORE && score <= MAXSCORE);
	assert(pos->ply >= 0 && pos->ply < MAXDEPTH);

	if (HashTable->pTable[index].posKey == 0) {
		if (score > ISMATE) score += pos->ply;
		else if (score < -ISMATE) score -= pos->ply;

		HashTable->pTable[index].move = move;
		HashTable->pTable[index].posKey = pos->posKey;
		HashTable->pTable[index].flags = flags;
		HashTable->pTable[index].score = score;
		HashTable->pTable[index].depth = depth;
		HashTable->newWrite++;
	}

	else if (depth >= HashTable->pTable[index].depth) {

		if (score > ISMATE) score += pos->ply;
		else if (score < -ISMATE) score -= pos->ply;

		HashTable->pTable[index].move = move;
		HashTable->pTable[index].posKey = pos->posKey;
		HashTable->pTable[index].flags = flags;
		HashTable->pTable[index].score = score;
		HashTable->pTable[index].depth = depth;
		HashTable->overWrite++;
	}


}

int ProbePvMove(const S_Board* pos) {

	int index = pos->posKey % HashTable->numEntries;
	assert(index >= 0 && index <= HashTable->numEntries - 1);

	if (HashTable->pTable[index].posKey == pos->posKey) {
		return HashTable->pTable[index].move;
	}

	return NOMOVE;
}


