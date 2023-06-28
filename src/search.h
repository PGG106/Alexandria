#pragma once

#include <cstdint>
#include "board.h"
#include "uci.h"

struct Search_stack {
    int excludedMove = { NOMOVE };
    int16_t static_eval = { 0 };
    int move = { 0 };
    int ply;
    int searchKillers[2] = { NOMOVE };
    int double_extensions = 0;
};

struct Search_data {
    int searchHistory[2][Board_sq_num][Board_sq_num] = { 0 };
    int CounterMoves[Board_sq_num][Board_sq_num] = { 0 };
    int cont_hist[12][64][12][64] = { 0 };
};

// a collection of all the data a thread needs to condut a search
struct S_ThreadData {
    int id = 0;
    S_Board pos;
    Search_data ss;
    S_SearchINFO info;
    PvTable pv_table;
    uint64_t nodeSpentTable[Board_sq_num][Board_sq_num] = { {0} };
    int RootDepth;
};

//ClearForSearch handles the cleaning of the thread data from a clean state
void ClearForSearch(S_ThreadData* td);

//Starts the search process, this is ideally the point where you can start a multithreaded search
void RootSearch(int depth, S_ThreadData* td, S_UciOptions* options);
// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search , calls the Negamax function and handles the console output
void SearchPosition(int start_depth, int final_depth, S_ThreadData* td, S_UciOptions* options);
//Sets up aspiration windows and starts a Negamax search
int AspirationWindowSearch(int prev_eval, int depth, S_ThreadData* td);
// Negamax alpha beta search
int Negamax(int alpha, int beta, int depth, bool cutnode, S_ThreadData* td, Search_stack* ss);
//Quiescence search to avoid the horizon effect
int Quiescence(int alpha, int beta, S_ThreadData* td, Search_stack* ss);

int GetBestMove(const PvTable* pv_table);

// inspired by the Weiss engine
bool SEE(const S_Board* pos, const int move, const int threshold);
//Checks if the current position is a draw
bool IsDraw(const S_Board* pos);
