#pragma once

#include "position.h"
#include "types.h"
#include <vector>

constexpr int ENTRIES_PER_BUCKET = 3;

// 10 bytes:
// 2 for move
// 2 for score
// 2 for eval
// 2 for key
// 1 for depth
// 1 for age + bound + PV
PACK(struct HashEntry {
    int16_t move = NOMOVE;
    int16_t score = SCORE_NONE;
    int16_t eval = SCORE_NONE;
    TTKey ttKey = 0;
    uint8_t depth = 0;
    uint8_t ageBoundPV = HFNONE; // lower 2 bits is bound, 3rd bit is PV, next 5 is age
});

// Packs the 10-byte entries into 32-byte buckets
// 3 entries per bucket with 2 bytes of padding
struct HashBucket {
    HashEntry entries[ENTRIES_PER_BUCKET] = {};
    uint16_t padding;
};

static_assert(sizeof(HashEntry) == 10);
static_assert(sizeof(HashBucket) == 32);

struct S_HashTable {
    std::vector<HashBucket> pTable;
    uint8_t age;
};

extern S_HashTable HashTable;

constexpr uint8_t MAX_AGE = 1 << 5; // must be power of 2
constexpr uint8_t AGE_MASK = MAX_AGE - 1;

void ClearHashTable();
// Initialize an Hashtable of size MB
void InitHashTable(uint64_t MB);

[[nodiscard]] bool ProbeHashEntry(const ZobristKey posKey, HashEntry* tte);

void StoreHashEntry(const ZobristKey key, const int16_t move, int score, int eval, const int bound, const int depth, const bool pv, const bool wasPV);

[[nodiscard]] uint64_t Index(const ZobristKey posKey);

int GetHashfull();

void TTPrefetch(const ZobristKey posKey);

int ScoreToTT(int score, int ply);

int ScoreFromTT(int score, int ply);

int16_t MoveToTT(int move);

int MoveFromTT(Position *pos, int16_t packed_move);

uint8_t BoundFromTT(uint8_t ageBoundPV);

bool FormerPV(uint8_t ageBoundPV);

uint8_t AgeFromTT(uint8_t ageBoundPV);

uint8_t PackToTT(uint8_t bound, bool wasPV, uint8_t age);

void UpdateEntryAge(uint8_t &ageBoundPV);

void UpdateTableAge();