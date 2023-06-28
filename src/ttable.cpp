// pvtable.c
#include "ttable.h"
#include "assert.h"
#include "io.h"
#include <cstring>
#include <iostream>

S_HashTable HashTable[1];

void ClearHashTable(S_HashTable* table) {
    std::fill(table->pTable.begin(), table->pTable.end(), S_HashEntry());
}

void InitHashTable(S_HashTable* table, uint64_t MB) {
    uint64_t HashSize = 0x100000 * MB;
    uint64_t numEntries = (HashSize / sizeof(S_HashEntry)) - 2;
    table->pTable.resize(numEntries);
    ClearHashTable(table);
    std::cout << "HashTable init complete with " << numEntries << " entries\n";
}

bool ProbeHashEntry(const S_Board* pos, S_HashEntry* tte) {
    uint64_t index = Index(pos->posKey);

    *tte = HashTable->pTable[index];

    return (HashTable->pTable[index].tt_key == static_cast<TTKey>(pos->posKey));
}

void StoreHashEntry(const ZobristKey key, const int move, int score, int16_t eval, const int flags,
    const int depth, const bool pv) {
    // Calculate index based on the position key and get the entry that already fills that index
    uint64_t index = Index(key);
    S_HashEntry* tte = &HashTable->pTable[index];

    // Replacement strategy taken from Stockfish
    // Preserve any existing move for the same position
    if (move || static_cast<TTKey>(key) != HashTable->pTable[index].tt_key)
        tte->move = move;

    // Overwrite less valuable entries (cheapest checks first)
    if (flags == HFEXACT || static_cast<TTKey>(key) != tte->tt_key || depth + 11 + 2 * pv > tte->depth) {
        tte->tt_key = static_cast<TTKey>(key);
        tte->flags = static_cast<uint8_t>(flags);
        tte->score = static_cast<int16_t>(score);
        tte->eval = eval;
        tte->depth = static_cast<uint8_t>(depth);
    }
}

uint64_t Index(const ZobristKey posKey) {
    return (static_cast<uint32_t>(posKey) * static_cast<uint64_t>(HashTable->pTable.size())) >> 32;
}

// prefetches the data in the given address in l1/2 cache in a non blocking way.
void prefetch(const void* addr) {
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
    _mm_prefetch((char*)addr, _MM_HINT_T0);
#else
    __builtin_prefetch(addr);
#endif
}

void TTPrefetch(const ZobristKey posKey) {
    prefetch(&HashTable->pTable[Index(posKey)]);
}
