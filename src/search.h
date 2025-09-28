#pragma once

#include <cstdint>
#include "position.h"
#include "uci.h"
#include "threads.h"

struct SearchStack {
    // don't init. search will init before entering the negamax method
    int16_t staticEval;
    Move excludedMove;
    Move move;
    uint16_t ply;
    Move searchKiller;
    int (*contHistEntry)[12 * 64];
    int16_t reduction;
};


struct PvTable {
    int pvLength[MAXDEPTH + 1];
    Move pvArray[MAXDEPTH + 1][MAXDEPTH + 1];
};

// These 2 tables need to be cleaned after each search. We initialize (and subsequently clean them) elsewhere
inline PvTable pvTable;
inline uint64_t nodeSpentTable[64 * 64];

// ClearForSearch handles the cleaning of the thread data from a clean state
void ClearForSearch(ThreadData* td);

// Starts the search process, this is ideally the point where you can start a multithreaded search
void RootSearch(int depth, ThreadData* td, UciOptions* options);

// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search , calls the Negamax function and handles the console output
void SearchPosition(int start_depth, int final_depth, ThreadData* td, UciOptions* options);

// Sets up aspiration windows and starts a Negamax search
[[nodiscard]] int AspirationWindowSearch(int prev_eval, int avg_eval,  int depth, ThreadData* td);

// Negamax alpha beta search
template <bool pvNode>
[[nodiscard]] int Negamax(int alpha, int beta, int depth, const bool cutNode, ThreadData* td, SearchStack* ss);

// Quiescence search to avoid the horizon effect
template <bool pvNode>
[[nodiscard]] int Quiescence(int alpha, int beta, ThreadData* td, SearchStack* ss);

// Gets best move from PV table
[[nodiscard]] Move GetBestMove();

// inspired by the Weiss engine
[[nodiscard]] bool SEE(const Position* pos, const Move move, const int threshold);

// Checks if the current position is a draw
[[nodiscard]] bool IsDraw(Position* pos);
