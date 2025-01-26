#pragma once

#include <atomic>
#include <cstdint>
#include "history.h"
#include "position.h"
#include "uci.h"

struct SearchStack {
    // don't init. search will init before entering the negamax method
    int16_t staticEval;
    Move excludedMove;
    Move move;
    int ply;
    Move searchKiller;
    int (*contHistEntry)[12 * 64];
};

struct SearchData {
    int searchHistory[2][64 * 64] = {};
    int rootHistory[2][64 * 64] = {};
    int captHist[12 * 64][6] = {};
    int counterMoves[64 * 64] = {};
    int contHist[12 * 64][12 * 64] = {};
    int pawnCorrHist[2][CORRHIST_SIZE] = {};
    int whiteNonPawnCorrHist[2][CORRHIST_SIZE] = {};
    int blackNonPawnCorrHist[2][CORRHIST_SIZE] = {};
    int contCorrHist[2][6 * 64][6 * 64] = {};
};

struct PvTable {
    int pvLength[MAXDEPTH + 1];
    Move pvArray[MAXDEPTH + 1][MAXDEPTH + 1];
};

// These 2 tables need to be cleaned after each search. We initialize (and subsequently clean them) elsewhere
inline PvTable pvTable;
inline uint64_t nodeSpentTable[64 * 64];

inline SearchInfo info;
inline std::atomic<bool> stopped = false;

inline bool stop(){
    return stopped.load(std::memory_order_relaxed);
}

inline void setStop(bool s){
    stopped.store(s, std::memory_order_relaxed);
}

// a collection of all the data a thread needs to conduct a search
struct ThreadData {
    int id = 0;
    Position pos;
    SearchData sd;
    int RootDepth;
    int nmpPlies;
    u64 nodes;
};

inline ThreadData mainTD;

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
[[nodiscard]] Move GetBestMove();

// inspired by the Weiss engine
[[nodiscard]] bool SEE(const Position* pos, const int move, const int threshold);

// Checks if the current position is a draw
[[nodiscard]] bool IsDraw(Position* pos);
