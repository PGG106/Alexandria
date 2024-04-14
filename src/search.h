#pragma once

#include <cstdint>
#include "position.h"
#include "uci.h"

struct SearchStack {
    // don't init, it will be init by search before entering the negamax method
    int excludedMove;
    int16_t staticEval;
    int move;
    int ply;
    int searchKillers[2];
    int doubleExtensions;
    int (*contHistEntry)[12 * 64];
};

struct SearchData {
    int searchHistory[2][64 * 64] = {};
    int captHist[12 * 64][6] = {};
    int counterMoves[64 * 64] = {};
    int contHist[12 * 64][12 * 64] = {};
};

// a collection of all the data a thread needs to conduct a search
struct ThreadData {
    int id = 0;
    Position pos;
    SearchData sd;
    SearchInfo info;
    // Since this 2 tables need to be cleaned after each search we just initialize (and subsequently clean them) elsewhere
    PvTable pvTable;
    uint64_t nodeSpentTable[64 * 64];
    int RootDepth;
    int nmpPlies;
};

// ClearForSearch handles the cleaning of the thread data from a clean state
void ClearForSearch(ThreadData* td);

// Starts the search process, this is ideally the point where you can start a multithreaded search
void RootSearch(int depth, ThreadData* td, UciOptions* options);

// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search , calls the Negamax function and handles the console output
void SearchPosition(int start_depth, int final_depth, ThreadData* td, UciOptions* options);

// Sets up aspiration windows and starts a Negamax search
[[nodiscard]] int AspirationWindowSearch(int prev_eval, int depth, ThreadData* td);

// Negamax alpha beta search
template <bool pvNode>
[[nodiscard]] int Negamax(int alpha, int beta, int depth, const bool cutNode, ThreadData* td, SearchStack* ss);

// Quiescence search to avoid the horizon effect
template <bool pvNode>
[[nodiscard]] int Quiescence(int alpha, int beta, ThreadData* td, SearchStack* ss);

// Gets best move from PV table
[[nodiscard]] int GetBestMove(const PvTable* pvTable);

// inspired by the Weiss engine
[[nodiscard]] bool SEE(const Position* pos, const int move, const int threshold);

// Checks if the current position is a draw
[[nodiscard]] bool IsDraw(Position* pos);
