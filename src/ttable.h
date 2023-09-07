#pragma once

#include "board.h"
#include "types.h"
#include <memory>
#include <vector>

PACK(struct S_HashEntry {
    int16_t move = NOMOVE;
    int16_t score = score_none;
    int16_t eval = score_none;
    TTKey tt_key = 0;
    uint8_t depth = 0;
    uint8_t wasPv_flags = HFNONE;
});

struct S_HashTable {
    std::vector<S_HashEntry> pTable;
};

extern S_HashTable HashTable[1];

void ClearHashTable(S_HashTable* table);
// Initialize an Hashtable of size MB
void InitHashTable(S_HashTable* table, uint64_t MB);

[[nodiscard]] bool ProbeHashEntry(const S_Board* pos, S_HashEntry* tte);

void StoreHashEntry(const ZobristKey key, const int16_t move, int score, int16_t eval, const int flags,
    const int depth, const bool pv, const bool wasPv);
[[nodiscard]] uint64_t Index(const ZobristKey posKey);
void TTPrefetch(const ZobristKey posKey);
int ScoreToTT(int score, int ply);

int ScoreFromTT(int score, int ply);

int16_t MoveToTT(int move);

int MoveFromTT(int16_t packed_move, int piece);