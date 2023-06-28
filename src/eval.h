#pragma once

struct S_Board;

//Returns if the position is a draw because there isn't enough mating material
bool MaterialDraw(const S_Board* pos);
// position evaluation
int EvalPosition(const S_Board* pos);
