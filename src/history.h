#pragma once

struct Position;
struct Search_data;
struct Search_stack;
struct MoveList;

// Functions used to update the history heuristics
void UpdateHistories(const Position* pos, Search_data* sd, Search_stack* ss, const int depth, const int bestMove, const MoveList* quietMoves, const MoveList* noisyMoves);
// Fuction that returns the history bonus
int history_bonus(const int depth);
// Getters for the history heuristics
[[nodiscard]] int GetHHScore(const Position* pos, const Search_data* sd, const int move);
[[nodiscard]] int GetCHScore(const Search_data* sd, const Search_stack* ss, const int move);
[[nodiscard]] int GetSingleCHScore(const Search_data* sd, const Search_stack* ss, const int move, const int offset);
[[nodiscard]] int GetHistoryScore(const Position* pos, const Search_data* sd, const int move, const Search_stack* ss);
[[nodiscard]] int GetCapthistScore(const Position* pos, const Search_data* sd, const int move);
// Clean all the history tables
void CleanHistories(Search_data* sd);
// Updates history heuristics for a single move
void updateHHScore(const Position* pos, Search_data* sd, int move, int bonus);
void updateCHScore(Search_data* sd, const Search_stack* ss, const int move, const int bonus);
void updateCapthistScore(const Position* pos, Search_data* sd, int move, int bonus);
void updateSingleCHScore(Search_data* sd, const Search_stack* ss, const int move, const int bonus, const int offset);
