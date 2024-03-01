#pragma once

#include "board.h"
#include "types.h"
#include <vector>

PACK(struct S_HashEntry {
    int16_t move = NOMOVE;
    int16_t score = SCORE_NONE;
    int16_t eval = SCORE_NONE;
    TTKey ttKey = 0;
    uint8_t depth = 0;
    uint8_t boundPV = HFNONE;
});

struct S_HashTable {
    std::vector<S_HashEntry> pTable;
};

extern S_HashTable HashTable[1];

void ClearHashTable(S_HashTable* table);
// Initialize an Hashtable of size MB
void InitHashTable(S_HashTable* table, uint64_t MB);

[[nodiscard]] bool ProbeHashEntry(const ZobristKey posKey, S_HashEntry* tte);

void StoreHashEntry(const ZobristKey key, const int16_t move, int score, int16_t eval, const int flag, const int depth, const bool pv, const bool wasPV);

[[nodiscard]] uint64_t Index(const ZobristKey posKey);

int GetHashfull();

void TTPrefetch(const ZobristKey posKey);

int ScoreToTT(int score, int ply);

int ScoreFromTT(int score, int ply);

int16_t MoveToTT(int move);

int MoveFromTT(S_Board *pos, int16_t packed_move);

uint8_t BoundFromTT(uint8_t boundPV);

bool FormerPV(uint8_t boundPV);

uint8_t PackToTT(uint8_t flag, bool wasPV);