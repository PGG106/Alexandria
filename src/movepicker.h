#pragma once
#include "search.h"
#include <cstdint>

struct MoveList;

enum {
    PICK_TT,
    GEN_NOISY,
    PICK_GOOD_NOISY,
    PICK_KILLER,
    PICK_COUNTER,
    GEN_QUIETS,
    PICK_QUIETS,
    GEN_BAD_NOISY,
    PICK_BAD_NOISY
};

enum MovepickerType : uint8_t {
    SEARCH,
    QSEARCH,
    PROBCUT
};

struct Movepicker {
    MovepickerType movepickerType;
    Position* pos;
    SearchData* sd;
    SearchStack* ss;
    MoveList moveList;
    int badcapturesCount;
    Move ttMove;
    Move killer;
    Move counter;
    int idx;
    int stage;
    int SEEThreshold;
    bool rootNode;
};

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const Move ttMove, const int SEEThreshold, const MovepickerType movepickerType, const bool rootNode);
Move NextMove(Movepicker* mp, const bool skip);

