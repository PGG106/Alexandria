#pragma once

struct Position;
struct SearchData;
struct SearchStack;
struct MoveList;

// Functions used to update the history heuristics
void UpdateHistories(const Position* pos, SearchData* sd, SearchStack* ss, const int depth, const int bestMove, const MoveList* quietMoves, const MoveList* noisyMoves);
// Fuction that returns the history bonus
int history_bonus(const int depth);
// Getters for the history heuristics
[[nodiscard]] int GetHHScore(const Position* pos, const SearchData* sd, const int move);
[[nodiscard]] int GetCHScore(const SearchStack* ss, const int move);
[[nodiscard]] int GetSingleCHScore(const SearchStack* ss, const int move, const int offset);
[[nodiscard]] int GetCapthistScore(const Position* pos, const SearchData* sd, const int move);
[[nodiscard]] int GetHistoryScore(const Position* pos, const SearchData* sd, const int move, const SearchStack* ss);
// Clean all the history tables
void CleanHistories(SearchData* sd);
// Updates history heuristics for a single move
void updateHHScore(const Position* pos, SearchData* sd, int move, int bonus);
void updateCHScore(SearchStack* ss, const int move, const int bonus);
void updateCapthistScore(const Position* pos, SearchData* sd, int move, int bonus);
void updateSingleCHScore(SearchStack* ss, const int move, const int bonus, const int offset);
