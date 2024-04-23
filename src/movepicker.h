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
    QSEARCH
};

struct Movepicker {
    MovepickerType movepickerType;
    Position* pos;
    SearchData* sd;
    SearchStack* ss;
    MoveList moveList;
    MoveList badCaptureList;
    int idx;
    int stage;
    int ttMove;
    int killer;
    int counter;
};

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const int ttMove, const MovepickerType movepickerType);
int NextMove(Movepicker* mp, const bool skip);
