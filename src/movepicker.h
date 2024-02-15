#pragma once
#include "search.h"

struct S_MOVELIST;

enum {
    PICK_TT,
    GEN_MOVES,
    PICK_MOVES
};

struct Movepicker {
    S_Board* pos;
    Search_data* sd;
    Search_stack* ss;
    S_MOVELIST moveList[1];
    int idx;
    int stage;
    int ttMove;
    int killer0;
    int killer1;
    int counter;
    bool capturesOnly;
    int SEEThreshold;
};

void InitMP(Movepicker* mp, S_Board* pos, Search_data* sd, Search_stack* ss, const int ttMove, const bool capturesOnly, const int SEEThreshold = score_none);
int NextMove(Movepicker* mp, const bool skipNonGood);
