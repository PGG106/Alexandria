#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "Board.h"
#include "move.h"
#include "search.h"


//Update the history heuristics of all the quiet moves passed to the function
void updateHH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves);

//Returns the history score of a move
int getHHScore(const S_Board* pos, const Search_data* ss, const int  move);