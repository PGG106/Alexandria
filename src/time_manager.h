#pragma once
#include "Board.h"

void optimum(S_SearchINFO* info, int time, int inc);
bool stopEarly(const S_SearchINFO* info);
bool timeOver(const S_SearchINFO* info);
bool QuickReturn(const S_Stack* ss, const S_SearchINFO* info);
