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
    GEN_QUIET_CHECKS,
    PICK_QUIET_CHECKS,
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
    Move ttMove;
    Move killer;
    Move counter;
    uint8_t idx;
    uint8_t stage;
    uint16_t badcapturesCount;
    int SEEThreshold;
    bool rootNode;
    bool genQuietChecks;
};

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const Move ttMove, const int SEEThreshold, const MovepickerType movepickerType, const bool rootNode);
Move NextMove(Movepicker* mp, const bool skip);

