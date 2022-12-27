#include "Board.h"
#include "move.h"
#pragma once
#include "Board.h"
#include "move.h"
#include "History.h"
#include <stdio.h>
#include <stdlib.h>

//Update the history heuristics of all the quiet moves passed to the function
void updateHH(const S_Board* pos, S_Stack* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves);

//Returns the history score of a move
int getHHScore(const S_Board* pos, const S_Stack* ss, const int  move);