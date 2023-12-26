#pragma once
#include "search.h"

struct S_MOVELIST;

enum {
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
    bool capturesOnly;
};

void InitMP(Movepicker* mp, S_Board* pos, Search_data* sd, Search_stack* ss, const int ttMove, const bool capturesOnly);
int NextMove(Movepicker* mp, const bool skipNonGood);
