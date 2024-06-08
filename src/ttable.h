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
PACK(struct TTEntry {
    PackedMove move = NOMOVE;
    int16_t score = SCORE_NONE;
    int16_t eval = SCORE_NONE;
    TTKey ttKey = 0;
    uint8_t depth = 0;
    uint8_t ageBoundPV = HFNONE; // lower 2 bits is bound, 3rd bit is PV, next 5 is age
});

// Packs the 10-byte entries into 32-byte buckets
// 3 entries per bucket with 2 bytes of padding
struct alignas(32) TTBucket {
    TTEntry entries[ENTRIES_PER_BUCKET] = {};
    uint16_t padding;
};

static_assert(sizeof(TTEntry) == 10);
static_assert(sizeof(TTBucket) == 32);

struct TTable {
    TTBucket *pTable = nullptr;
    uint64_t numBuckets;
    size_t paddedSize;
    uint8_t age;
};

extern TTable TT;

constexpr uint8_t MAX_AGE = 1 << 5; // must be power of 2
constexpr uint8_t AGE_MASK = MAX_AGE - 1;

void* AlignedMalloc(size_t size, size_t alignment);

void AlignedFree(void *src);

void ClearTT();
// Initialize an TT of size MB
void InitTT(uint64_t MB);

[[nodiscard]] bool ProbeTTEntry(const ZobristKey posKey, TTEntry* tte);

void StoreTTEntry(const ZobristKey key, const PackedMove move, int score, int eval, const int bound, const int depth, const bool pv, const bool wasPV);

[[nodiscard]] uint64_t Index(const ZobristKey posKey);

[[nodiscard]] int GetHashfull();

void TTPrefetch(const ZobristKey posKey);

int ScoreToTT(int score, int ply);

int ScoreFromTT(int score, int ply);

PackedMove MoveToTT(Move move);

Move MoveFromTT(Position *pos, PackedMove packed_move);

uint8_t BoundFromTT(uint8_t ageBoundPV);

bool FormerPV(uint8_t ageBoundPV);

uint8_t AgeFromTT(uint8_t ageBoundPV);

uint8_t PackToTT(uint8_t bound, bool wasPV, uint8_t age);

void UpdateTableAge();