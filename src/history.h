#pragma once

struct S_Board;
struct Search_data;
struct Search_stack;
struct S_MOVELIST;

// Functions used to update the history heuristics
void UpdateHistories(const S_Board* pos, Search_data* sd, Search_stack* ss, const int depth, const int bestMove, const S_MOVELIST* quietMoves, const S_MOVELIST* noisyMoves);
// Fuction that returns the history bonus
int history_bonus(const int depth);
// Getters for the history heuristics
[[nodiscard]] int GetHHScore(const S_Board* pos, const Search_data* sd, const int move);
[[nodiscard]] int GetCHScore(const Search_data* sd, const Search_stack* ss, const int move);
[[nodiscard]] int GetSingleCHScore(const Search_data* sd, const Search_stack* ss, const int move, const int offset);
[[nodiscard]] int GetHistoryScore(const S_Board* pos, const Search_data* sd, const int move, const Search_stack* ss);
[[nodiscard]] int GetCapthistScore(const S_Board* pos, const Search_data* sd, const int move);
// Clean all the history tables
void CleanHistories(Search_data* sd);
// Updates history heuristics for a single move
void updateHHScore(const S_Board* pos, Search_data* sd, int move, int bonus);
void updateCHScore(Search_data* sd, const Search_stack* ss, const int move, const int bonus);
void updateCapthistScore(const S_Board* pos, Search_data* sd, int move, int bonus);
void updateSingleCHScore(Search_data* sd, const Search_stack* ss, const int move, const int bonus, const int offset);
