#pragma once

#include <algorithm>
#include <cstring>
#include "position.h"
#include "move.h"
#include "types.h"

struct Position;
struct SearchData;
struct SearchStack;
struct MoveList;

int16_t HistoryBonus(const int depth);
void UpdateHistoryEntry(int16_t &entry, const int16_t bonus, const int16_t max);

// Quiet history is a history table indexed by [side-to-move][from-to-of-move].
struct QuietHistoryTable {
    struct QuietHistoryEntry {
        int16_t factoriser;
        int16_t buckets[2][2]; // Buckets indexed by [from-sq-is-attacked][to-sq-is-attacked]

        inline int16_t &bucketRef(const Position *pos, const Move move) {
            return buckets[IsAttackedByOpp(pos, From(move))][IsAttackedByOpp(pos, To(move))];
        };

        inline int16_t bucket(const Position *pos, const Move move) const {
            return buckets[IsAttackedByOpp(pos, From(move))][IsAttackedByOpp(pos, To(move))];
        };
    };
    QuietHistoryEntry table[2][64 * 64];

    void update(const Position *pos, const Move move, const int16_t bonus);
    int16_t getScore(const Position *pos, const Move move) const;
    inline void clear() {
        std::memset(table, 0, sizeof(table));
    };
};

// Update all histories after a beta cutoff
void UpdateAllHistories(const Position *pos, const SearchStack *ss, SearchData *sd, const int depth, const Move bestMove, const MoveList &quietMoves, const MoveList &tacticalMoves);

// Get history score for a given move
int GetHistoryScore(const Position *pos, const SearchStack *ss, const SearchData *sd, const Move move);

// Clean all the history tables
void CleanHistories(SearchData* sd);
