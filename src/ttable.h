#pragma once

#include "board.h"
#include "types.h"
#include <memory>
#include <vector>

PACK(struct S_HashEntry {
    int32_t move = NOMOVE;
    int16_t score = 0;
    int16_t eval = 0;
    TTKey tt_key = 0;
    uint8_t depth = 0;
    uint8_t flags = HFNONE;
    bool wasPv = false;
});

struct S_HashTable {
    std::vector<S_HashEntry> pTable;
};

extern S_HashTable HashTable[1];

void ClearHashTable(S_HashTable* table);
// Initialize an Hashtable of size MB
void InitHashTable(S_HashTable* table, uint64_t MB);

[[nodiscard]] bool ProbeHashEntry(const S_Board* pos, S_HashEntry* tte);

void StoreHashEntry(const ZobristKey key, const int move, int score, int16_t eval, const int flags,
    const int depth, const bool pv, const bool wasPv);
[[nodiscard]] uint64_t Index(const ZobristKey posKey);
void TTPrefetch(const ZobristKey posKey);
