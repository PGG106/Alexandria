#pragma once

#include <cstdint>
#include "history.h"
#include "position.h"
#include "uci.h"

constexpr int LMR_GRAIN = 1024;

inline int seeMargins[2][64];
inline int futilityMargins[2][64];
inline int lmrReductions[2][64][64];
inline int lmpMargins[2][64];

struct SearchStack {
    int ply;
    int staticEval;
    Move move;
};

struct SearchData {
    QuietHistoryTable        quietHistory;
    TacticalHistoryTable     tacticalHistory;
    ContinuationHistoryTable continuationHistory;
    CorrectionHistoryTable   correctionHistory;
};

struct PvTable {
    int pvLength[MAXDEPTH + 1];
    Move pvArray[MAXDEPTH + 1][MAXDEPTH + 1];
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
};

// ClearForSearch handles the cleaning of the thread data from a clean state
void ClearForSearch(ThreadData* td);

// Starts the search process, this is ideally the point where you can start a multithreaded search
void RootSearch(int depth, ThreadData* td, UciOptions* options);

// SearchPosition is the actual function that handles the search, it sets up the variables needed for the search , calls the Negamax function and handles the console output
void SearchPosition(int start_depth, int final_depth, ThreadData* td, UciOptions* options);

// Sets up aspiration windows and starts a Negamax search
[[nodiscard]] int AspirationWindowSearch(int prev_eval, int depth, ThreadData* td);

// Adjusts eval and sets the different eval variables
void adjustEval(Position *pos, SearchData *sd, int rawEval, int &staticEval, int &eval, int ttScore, uint8_t ttBound);

// Negamax alpha beta search
template <bool pvNode>
[[nodiscard]] int Negamax(int alpha, int beta, int depth, bool predictedCutNode, ThreadData* td, SearchStack* ss, Move excludedMove = NOMOVE);

// Quiescence search to avoid the horizon effect
template <bool pvNode>
[[nodiscard]] int Quiescence(int alpha, int beta, ThreadData* td, SearchStack* ss);

// Gets best move from PV table
[[nodiscard]] Move GetBestMove(const PvTable* pvTable);

// inspired by the Weiss engine
[[nodiscard]] bool SEE(const Position* pos, const int move, const int threshold);

// Checks if the current position is a draw
[[nodiscard]] bool IsDraw(Position* pos);
