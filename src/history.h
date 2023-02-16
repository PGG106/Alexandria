#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "board.h"
#include "move.h"
#include "search.h"


//Update the history heuristics of all the quiet moves passed to the function
void UpdateHH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves);
void UpdateCH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const int previous_move, const int previous_previous_move, const S_MOVELIST* quiet_moves);
//Returns the history score of a move
int GetHHScore(const S_Board* pos, const Search_data* sd, const int  move);
int64_t GetCHScore(const S_Board* pos, const Search_data* sd, const int  move, const int previous_move, const int previous_previous_move);
void CleanHistories(Search_data* ss);
