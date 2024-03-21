#pragma once

#include <cstdint>
#include "position.h"
#include "uci.h"

struct SearchStack {
    int excludedMove = {};
    int16_t staticEval = {};
    int move = {};
    int ply;
    int searchKillers[2] = {};
    int doubleExtensions = 0;
};

struct SearchData {
    int searchHistory[2][Board_sq_num][Board_sq_num] = {};
    // Piece, To, Captured
    int captHist[12][Board_sq_num][6] = {};
    int counterMoves[Board_sq_num][Board_sq_num] = {};
    int contHist[12][64][12][64] = {};
};

// a collection of all the data a thread needs to conduct a search
struct S_ThreadData {
    int id = 0;
    Position pos;
    SearchData sd;
    SearchInfo info;
    PvTable pvTable;
    uint64_t nodeSpentTable[Board_sq_num][Board_sq_num] = {};
    int RootDepth;
    int nmpPlies;
};

// ClearForSearch handles the cleaning of the thread data from a clean state
void ClearForSearch(S_ThreadData* td);

// Starts the search process, this is ideally the point where you can start a multithreaded search
void RootSearch(int depth, S_ThreadData* td, S_UciOptions* options);

// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search , calls the Negamax function and handles the console output
void SearchPosition(int start_depth, int final_depth, S_ThreadData* td, S_UciOptions* options);

// Sets up aspiration windows and starts a Negamax search
[[nodiscard]] int AspirationWindowSearch(int prev_eval, int depth, S_ThreadData* td);

// Negamax alpha beta search
template <bool pvNode>
[[nodiscard]] int Negamax(int alpha, int beta, int depth, const bool cutNode, S_ThreadData* td, SearchStack* ss);

// Quiescence search to avoid the horizon effect
template <bool pvNode>
[[nodiscard]] int Quiescence(int alpha, int beta, S_ThreadData* td, SearchStack* ss);

// Gets best move from PV table
[[nodiscard]] int GetBestMove(const PvTable* pvTable);

// inspired by the Weiss engine
[[nodiscard]] bool SEE(const Position* pos, const int move, const int threshold);

// Checks if the current position is a draw
[[nodiscard]] bool IsDraw(Position* pos);
