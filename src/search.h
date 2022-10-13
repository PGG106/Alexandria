#pragma once
#include "Board.h"
#include "PieceData.h"
#include "eval.h"
#include "io.h"
#include "makemove.h"
#include "misc.h"
#include "move.h"
#include "movegen.h"

//ClearForSearch handles the cleaning of the post and the info parameters to start search from a clean state
void ClearForSearch(S_Board* pos, S_Stack* ss, S_SearchINFO* info);

//Quiescence search to avoid the horizon effect
int Quiescence(int alpha, int beta, S_Board* pos, S_Stack* ss, S_SearchINFO* info);

// negamax alpha beta search
int negamax(int alpha, int beta, int depth, S_Board* pos, S_Stack* ss, S_SearchINFO* info, int DoNull);

//Starts the search process, this is ideally the point where you can start a multithreaded search
void Root_search_position(int depth, S_Board* pos, S_Stack* ss, S_SearchINFO* info);

// search_position is the actual function that handles the search, it sets up the variables needed for the search , calls the negamax function and handles the console output
void search_position(int start_depth, int final_depth, S_Board* pos, S_Stack* ss, S_SearchINFO* info, int show);
