#include "ttable.h"
#include "io.h"
#include <iostream>
// This include breaks on non x86 target platforms
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
#include "xmmintrin.h"
#endif

TTable TT;

void ClearTT() {
    std::fill(TT.pTable.begin(), TT.pTable.end(), TTBucket());
    TT.age = 1;
}

void InitTT(uint64_t MB) {
    const uint64_t hashSize = 0x100000 * MB;
    const uint64_t numBuckets = (hashSize / sizeof(TTBucket)) - 3;
    TT.pTable.resize(numBuckets);
    ClearTT();
    std::cout << "TT init complete with " << numBuckets << " buckets and " << numBuckets * ENTRIES_PER_BUCKET << " entries\n";
}

bool ProbeTTEntry(const ZobristKey posKey, TTEntry *tte) {

    const uint64_t index = Index(posKey);
    TTBucket *bucket = &TT.pTable[index];
    for (int i = 0; i < ENTRIES_PER_BUCKET; i++) {
        *tte = bucket->entries[i];
        if (tte->ttKey == static_cast<TTKey>(posKey)) {
            return true;
        }
    }
    return false;
}

void StoreTTEntry(const ZobristKey key, const int16_t move, int score, int eval, const int bound, const int depth, const bool pv, const bool wasPV) {
    // Calculate index based on the position key and get the entry that already fills that index
    const uint64_t index = Index(key);
    const TTKey key16 = static_cast<TTKey>(key);
    const uint8_t TTAge = TT.age;
    TTBucket* bucket = &TT.pTable[index];
    TTEntry* tte = &bucket->entries[0];
    for (int i = 0; i < ENTRIES_PER_BUCKET; i++) {
        TTEntry* entry = &bucket->entries[i];

        if (entry->ttKey == key16) {
            tte = entry;
            break;
        }

        if (tte->depth - ((MAX_AGE + TTAge - AgeFromTT(tte->ageBoundPV)) & AGE_MASK) * 4
            > entry->depth - ((MAX_AGE + TTAge - AgeFromTT(entry->ageBoundPV)) & AGE_MASK) * 4) {
            tte = entry;
        }
    }

    // Replacement strategy taken from Stockfish
    // Preserve any existing move for the same position
    if (move || key16 != tte->ttKey)
        tte->move = move;

    // Overwrite less valuable entries (cheapest checks first)
    if (   bound == HFEXACT
        || key16 != tte->ttKey
        || depth + 5 + 2 * pv > tte->depth
        || AgeFromTT(tte->ageBoundPV) != TTAge) {
        tte->ttKey = key16;
        tte->ageBoundPV = PackToTT(bound, wasPV, TTAge);
        tte->score = static_cast<int16_t>(score);
        tte->eval = static_cast<int16_t>(eval);
        tte->depth = static_cast<uint8_t>(depth);
    }
}

int GetHashfull() {
    int hit = 0;
    for (int i = 0; i < 2000; i++) {
        const TTBucket *bucket = &TT.pTable[i];
        for (int idx = 0; idx < ENTRIES_PER_BUCKET; idx++) {
            const TTEntry *tte = &bucket->entries[idx];
            if (tte->ttKey != 0 && AgeFromTT(tte->ageBoundPV) == TT.age)
                hit++;
        }
    }
    return hit / (2 * ENTRIES_PER_BUCKET);
}

uint64_t Index(const ZobristKey posKey) {
#ifdef __SIZEOF_INT128__
    return static_cast<uint64_t>(((static_cast<__uint128_t>(posKey) * static_cast<__uint128_t>(TT.pTable.size())) >> 64));
#else
    // Workaround to use the correct bits when indexing the TT on a platform with no int128 support, code s̶t̶o̶l̶e̶n̶ f̶r̶o̶m̶ provided by Nanopixel
    uint64_t xlo = static_cast<uint32_t>(posKey);
    uint64_t xhi = posKey >> 32;
    uint64_t nlo = static_cast<uint32_t>(TT.pTable.size());
    uint64_t nhi = TT.pTable.size() >> 32;
    uint64_t c1 = (xlo * nlo) >> 32;
    uint64_t c2 = (xhi * nlo) + c1;
    uint64_t c3 = (xlo * nhi) + static_cast<uint32_t>(c2);

    return xhi * nhi + (c2 >> 32) + (c3 >> 32);
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
    prefetch(&TT.pTable[Index(posKey)].entries[0]);
}


int ScoreToTT(int score, int ply) {
    if (score > MATE_FOUND)
        score += ply;
    else if (score < -MATE_FOUND)
        score -= ply;

    return score;
}

int ScoreFromTT(int score, int ply) {
    if (score > MATE_FOUND)
        score -= ply;
    else if (score < -MATE_FOUND)
        score += ply;

    return score;
}

int16_t MoveToTT(Move move) {
    return (move & 0xffff);
}

Move MoveFromTT(Position *pos, int16_t packed_move) {
    // It's important to preserve a move being null even it's being unpacked
    if (packed_move == NOMOVE)
        return NOMOVE;

    const int piece = pos->PieceOn(From(packed_move));
    return (static_cast<Move>(static_cast<uint16_t>(packed_move)) | (piece << 16));
}

uint8_t BoundFromTT(uint8_t ageBoundPV) {
    return ageBoundPV & 0b00000011;
}

bool FormerPV(uint8_t ageBoundPV) {
    return ageBoundPV & 0b00000100;
}

uint8_t AgeFromTT(uint8_t ageBoundPV) {
    return (ageBoundPV & 0b11111000) >> 3;
}

uint8_t PackToTT(uint8_t bound, bool wasPV, uint8_t age) {
    return static_cast<uint8_t>(bound + (wasPV << 2) + (age << 3));
}

void UpdateTableAge() {
    TT.age = (TT.age + 1) & AGE_MASK;
}
