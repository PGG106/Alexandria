#pragma once

#include "types.h"
#include "move.h"

struct Position;
struct SearchData;
struct SearchStack;
struct MoveList;

constexpr int HH_MAX = 8192;
constexpr int RH_MAX = 8192;
constexpr int CH_MAX = 16384;
constexpr int CAPTHIST_MAX = 16384;
constexpr int CORRHIST_SIZE = 32768;
constexpr int CORRHIST_MAX = 1024;
constexpr int CORRHIST_GRAIN = 256;

// Functions used to update the history heuristics
void UpdateHistories(const Position* pos, SearchData* sd, SearchStack* ss, const int depth, const Move bestMove, const StackMoveList* quietMoves, const StackMoveList* noisyMoves, const bool rootNode);
// Fuction that returns the history bonus
int history_bonus(const int depth);
int history_malus(const int depth);
// Getters for the history heuristics
[[nodiscard]] int GetHHScore(const Position* pos, const SearchData* sd, const Move move);
[[nodiscard]] int GetRHScore(const Position* pos, const SearchData* sd, const Move move);
[[nodiscard]] int GetCHScore(const SearchStack* ss, const Move move);
[[nodiscard]] int GetSingleCHScore(const SearchStack* ss, const Move move, const int offset);
[[nodiscard]] int GetCapthistScore(const Position* pos, const SearchData* sd, const Move move);
[[nodiscard]] int GetHistoryScore(const Position* pos, const SearchData* sd, const Move move, const SearchStack* ss, const bool rootNode);
[[nodiscard]] int GetCorrHistAdjustment(const Position *pos, const SearchData *sd, const SearchStack *ss);
// Clean all the history tables
void CleanHistories(SearchData* sd);
// Updates history heuristics for a single move
void updateHHScore(const Position* pos, SearchData* sd, const Move move, int bonus);
void updateOppHHScore(const Position* pos, SearchData* sd, const Move move, int bonus);
void updateCHScore(SearchStack* ss, const Move move, const int bonus);
void updateCapthistScore(const Position* pos, SearchData* sd, const Move move, int bonus);
void updateSingleCHScore(SearchStack* ss, const Move move, const int bonus, const int offset);

// Corrhist stuff
void updateCorrHistScore(const Position *pos, SearchData *sd, const SearchStack* ss, const int depth, const int diff);
int adjustEval(const Position *pos, const int correction, const int rawEval);

