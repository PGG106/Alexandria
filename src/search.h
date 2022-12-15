#pragma once
#include "Board.h"
#include "uci.h"

// a collection of all the data a thread needs to condut a search
typedef struct ThreadData {
	int id = 0;
	S_Board pos;
	S_Stack ss;
	S_SearchINFO info;
} S_ThreadData;

//ClearForSearch handles the cleaning of the thread data from a clean state
void ClearForSearch(S_ThreadData* td);

//Starts the search process, this is ideally the point where you can start a multithreaded search
void Root_search_position(int depth, S_ThreadData* td, S_UciOptions* options);
// search_position is the actual function that handles the search, it sets up the variables needed for the search , calls the negamax function and handles the console output
void search_position(int start_depth, int final_depth, S_ThreadData* td, S_UciOptions* options);
//Sets up aspiration windows and starts a negamax search
int aspiration_window_search(int prev_eval, int depth, S_ThreadData* td);
// negamax alpha beta search
int negamax(int alpha, int beta, int depth, S_ThreadData* td);
//Quiescence search to avoid the horizon effect
int Quiescence(int alpha, int beta, S_ThreadData* td);

int getBestMove(S_Stack* ss);

// inspired by the Weiss engine
bool SEE(const S_Board* pos, const int move, const int threshold);