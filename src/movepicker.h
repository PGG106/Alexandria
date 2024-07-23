#pragma once
#include "search.h"
#include <cstdint>

struct MoveList;

enum {
    PICK_TT,
    GEN_TACTICAL,
    PICK_GOOD_TACTICAL,
    GEN_QUIETS,
    PICK_QUIETS,
    GEN_BAD_TACTICAL,
    PICK_BAD_TACTICAL
};

enum MovepickerType : uint8_t {
    SEARCH,
    QSEARCH
};

struct Movepicker {
    MovepickerType movepickerType;
    Position* pos;
    SearchData* sd;
    SearchStack* ss;
    MoveList moveList;
    MoveList badCaptureList;
    Move ttMove;
    int idx;
    int stage;
};

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const Move ttMove, const MovepickerType movepickerType);
Move NextMove(Movepicker* mp, const bool skip);

