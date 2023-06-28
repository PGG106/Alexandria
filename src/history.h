#pragma once

struct S_Board;
struct Search_data;
struct Search_stack;
struct S_MOVELIST;

// Update the history heuristics of all the quiet moves passed to the function
void UpdateHH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves);
void UpdateCH(Search_data* sd, const Search_stack* ss, const int depth, const int move, const S_MOVELIST* quiet_moves);
// Returns the history score of a move
[[nodiscard]] int GetHHScore(const S_Board* pos, const Search_data* sd, const int move);
[[nodiscard]] int GetCHScore(const Search_data* sd, const Search_stack* ss, const int move);
[[nodiscard]] int GetHistoryScore(const S_Board* pos, const Search_data* sd, const int move, const Search_stack* ss);
void CleanHistories(Search_data* ss);
