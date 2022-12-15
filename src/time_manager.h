#pragma once
#include "Board.h"

void optimum(S_Board *pos, S_SearchINFO *info, int time, int inc);
bool stopEarly(S_SearchINFO* info);
bool timeOver(S_SearchINFO* info);
