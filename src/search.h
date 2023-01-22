#pragma once
#include "Board.h"
#include "uci.h"

struct Search_stack {
	int excludedMove = { NOMOVE };
	int eval = { 0 };
	int move = { 0 };
	int searchKillers[2] = { NOMOVE };
};

struct Search_data {
	int searchHistory[12][Board_sq_num] = { 0 };
	int CounterMoves[Board_sq_num][Board_sq_num] = { 0 };
	int64_t cont_hist[12][64][12][64] = { 0 };
};

// a collection of all the data a thread needs to condut a search
typedef struct ThreadData {
	int id = 0;
	S_Board pos;
	Search_data ss;
	S_SearchINFO info;
	PvTable pv_table;
}S_ThreadData;

//ClearForSearch handles the cleaning of the thread data from a clean state
void ClearForSearch(S_ThreadData* td);

//Starts the search process, this is ideally the point where you can start a multithreaded search
void Root_search_position(int depth, S_ThreadData* td, S_UciOptions* options);
// search_position is the actual function that handles the search, it sets up the variables needed for the search , calls the negamax function and handles the console output
void search_position(int start_depth, int final_depth, S_ThreadData* td, S_UciOptions* options);
//Sets up aspiration windows and starts a negamax search
int aspiration_window_search(int prev_eval, int depth, S_ThreadData* td);
// negamax alpha beta search
int negamax(int alpha, int beta, int depth, S_ThreadData* td, Search_stack* ss);
//Quiescence search to avoid the horizon effect
int Quiescence(int alpha, int beta, S_ThreadData* td, Search_stack* ss);

int getBestMove(const PvTable* pv_table);

// inspired by the Weiss engine
bool SEE(const S_Board* pos, const int move, const int threshold);