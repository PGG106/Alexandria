#pragma once
#include "board.h"

//Returns if the position is a draw because there isn't enough mating material
int MaterialDraw(const S_Board* pos);
// position evaluation
int EvalPosition(const S_Board* pos);
