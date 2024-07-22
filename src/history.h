#pragma once

#include "types.h"

struct Position;
struct SearchData;
struct SearchStack;
struct MoveList;

int16_t HistoryBonus(const int depth);

// Quiet history is a history table indexed by the side-to-move as well as a quiet move's [from-to].
void UpdateQuietHistory(const Position *pos, SearchData *sd, const Move move, const int16_t bonus);
int16_t GetQuietHistoryScore(const Position *pos, const SearchData *sd, const Move move);

// Update all histories after a beta cutoff
void UpdateAllHistories(const Position *pos, const SearchStack *ss, SearchData *sd, const int depth, const Move bestMove, const MoveList &quietMoves, const MoveList &tacticalMoves);

// Get history score for a given move
int GetHistoryScore(const Position *pos, const SearchStack *ss, const SearchData *sd, const Move move);

// Clean all the history tables
void CleanHistories(SearchData* sd);
