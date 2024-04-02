#pragma once
#include "search.h"

struct MoveList;

enum {
    PICK_TT,
    GEN_NOISY,
    PICK_GOOD_NOISY,
    PICK_KILLER_0,
    PICK_KILLER_1,
    PICK_COUNTER,
    GEN_QUIETS,
    PICK_QUIETS,
    GEN_BAD_NOISY,
    PICK_BAD_NOISY
};

struct Movepicker {
    Position* pos;
    SearchData* sd;
    SearchStack* ss;
    MoveList moveList;
    MoveList badCaptureList;
    int idx;
    int stage;
    int ttMove;
    int killer0;
    int killer1;
    int counter;
    bool capturesOnly;
    int SEEThreshold;
};

void InitMP(Movepicker* mp, Position* pos, SearchData* sd, SearchStack* ss, const int ttMove, const bool capturesOnly, const int SEEThreshold = SCORE_NONE);
int NextMove(Movepicker* mp, const bool skipNonGood);
