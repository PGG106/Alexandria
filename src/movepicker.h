#pragma once
#include "search.h"

struct MoveList;

enum {
    PICK_TT,
    GEN_MOVES,
    PICK_MOVES
};

struct Movepicker {
    Position* pos;
    SearchData* sd;
    SearchStack* ss;
    MoveList moveList;
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
