constexpr int TargetMap[5][6] = {
    {0, 1, -1, 2, -1, -1}, // pawn:   P N R
    {0, 1,  2, 3,  4, -1}, // knight: P N B R Q
    {0, 1,  2, 3, -1, -1}, // bishop: P N B R
    {0, 1,  2, 3, -1, -1}, // rook:   P N B R
    {0, 1,  2, 3,  4, -1}, // queen:  P N B R Q
};
constexpr int ThreatTargetCount[5] = {3, 5, 4, 4, 5};
constexpr int ThreatCount[5]  = {84, 336, 560, 896, 1456};
constexpr int ThreatOffset[5] = {0, 504, 3864, 8344, 15512};