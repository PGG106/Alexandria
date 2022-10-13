#include "Board.h"
#include "move.h"
#pragma once
#include "Board.h"
#include "move.h"
#include "History.h"
#include <stdio.h>
#include <stdlib.h>

//Update the history heuristics of all the quiet moves passed to the function
void updateHH(S_Board* pos, S_Stack* ss, int depth, int bestmove, S_MOVELIST* quiet_moves);

//Returns the history score of a move
int getHHScore(S_Board* pos, S_Stack* ss, int  move);