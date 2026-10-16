#include "nnue.h"
constexpr int TargetMap[5][6] = {
    {-1, 0, -1, 1, -1, -1}, // pawn:   N R (pawn on pawn is covered by the pawn pair inputs)
    { 0, 1,  2, 3,  4, -1}, // knight: P N B R Q
    { 0, 1,  2, 3, -1, -1}, // bishop: P N B R
    { 0, 1,  2, 3, -1, -1}, // rook:   P N B R
    { 0, 1,  2, 3,  4, -1}, // queen:  P N B R Q
};
constexpr int ThreatTargetCount[5] = {2, 5, 4, 4, 5};
constexpr int ThreatCount[5]  = {84, 336, 560, 896, 1456};
constexpr int ThreatOffset[5] = {0, 336, 3696, 8176, 15344};

// Active threat list for each side
struct ThreatFeatures {
    uint16_t indices[2][MAX_THREAT_FEATURES];
    int count[2] = {};
};