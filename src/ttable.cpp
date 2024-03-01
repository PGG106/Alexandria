#include "ttable.h"
#include "assert.h"
#include "io.h"
#include "xmmintrin.h"
#include <cstring>
#include <iostream>

S_HashTable HashTable[1];

void ClearHashTable(S_HashTable* table) {
    std::fill(table->pTable.begin(), table->pTable.end(), S_HashEntry());
}

void InitHashTable(S_HashTable* table, uint64_t MB) {
    const uint64_t hashSize = 0x100000 * MB;
    const uint64_t numEntries = (hashSize / sizeof(S_HashEntry)) - 2;
    table->pTable.resize(numEntries);
    ClearHashTable(table);
    std::cout << "HashTable init complete with " << numEntries << " entries\n";
}

bool ProbeHashEntry(const ZobristKey posKey, S_HashEntry* tte){
    const uint64_t index = Index(posKey);

    *tte = HashTable->pTable[index];

    return (HashTable->pTable[index].tt_key == static_cast<TTKey>(posKey));
}

void StoreHashEntry(const ZobristKey key, const int16_t move, int score, int16_t eval, const int flag, const int depth, const bool pv, const bool wasPv) {
    // Calculate index based on the position key and get the entry that already fills that index
    const uint64_t index = Index(key);
    S_HashEntry* tte = &HashTable->pTable[index];

    // Replacement strategy taken from Stockfish
    // Preserve any existing move for the same position
    if (move || static_cast<TTKey>(key) != HashTable->pTable[index].tt_key)
        tte->move = move;

    // Overwrite less valuable entries (cheapest checks first)
    if (flag == HFEXACT || static_cast<TTKey>(key) != tte->tt_key || depth + 5 + 2 * pv > tte->depth) {
        tte->tt_key = static_cast<TTKey>(key);
        tte->boundPV = PackToTT(flag, wasPv);
        tte->score = static_cast<int16_t>(score);
        tte->eval = eval;
        tte->depth = static_cast<uint8_t>(depth);
    }
}

int GetHashfull() {
    int hit = 0;
    for (int i = 0; i < 2000; i++) {
        S_HashEntry *tte = &HashTable->pTable[i];
        if (tte->tt_key != 0)
            hit++;
    }
    return hit / 2;
}

uint64_t Index(const ZobristKey posKey) {
#ifdef __SIZEOF_INT128__
    return static_cast<uint64_t>(((static_cast<__uint128_t>(posKey) * static_cast<__uint128_t>(HashTable->pTable.size())) >> 64));
#else 
    return posKey % HashTable->pTable.size();
#endif
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


int ScoreToTT(int score, int ply) {
    if (score > mate_score)
        score += ply;
    else if (score < -mate_score)
        score -= ply;

    return score;
}

int ScoreFromTT(int score, int ply) {
    if (score > mate_score)
        score -= ply;
    else if (score < -mate_score)
        score += ply;

    return score;
}

int16_t MoveToTT(int move) {
    return (move & 0xffff);
}

int MoveFromTT(S_Board *pos, int16_t packed_move) {
    // It's important to preserve a move being null even it's being unpacked
    if (packed_move == NOMOVE)
        return NOMOVE;

    int piece = pos->PieceOn(From(packed_move));
    return (packed_move | (piece << 16));
}

uint8_t BoundFromTT(uint8_t boundPV) {
    return boundPV & 0b00000011;
}

bool FormerPV(uint8_t boundPV) {
    return boundPV & 0b00000100;
}

uint8_t PackToTT(uint8_t flag, bool wasPv) {
    return static_cast<uint8_t>(flag + (wasPv << 2));
}