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

// Quiet history is a history table for quiet moves
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

    // Indexed by [side-to-move][from-to-of-move]
    QuietHistoryEntry table[2][64 * 64];

    inline QuietHistoryEntry getEntry(const Position *pos, const Move move) const {
        return table[pos->side][FromTo(move)];
    };

    inline QuietHistoryEntry &getEntryRef(const Position *pos, const Move move) {
        return table[pos->side][FromTo(move)];
    };

    inline void clear() {
        std::memset(table, 0, sizeof(table));
    };

    void update(const Position *pos, const Move move, const int16_t bonus);
    int16_t getScore(const Position *pos, const Move move) const;
};

// Tactical history is a history table for tactical moves
struct TacticalHistoryTable {
    struct TacticalHistoryEntry {
        int16_t factoriser;
    };

    // Indexed by [moved-piece][to-square][captured-piece]
    TacticalHistoryEntry table[12 * 64][6];

    inline TacticalHistoryEntry getEntry(const Position *pos, const Move move) const {
        int capturedPiece = GetPieceType(pos->PieceOn(To(move)));
        if (capturedPiece == EMPTY) capturedPiece = KING; // Impossible to capture kings so we use it as "Empty" slot to save space
        return table[PieceTo(move)][capturedPiece];
    };

    inline TacticalHistoryEntry &getEntryRef(const Position *pos, const Move move) {
        int capturedPiece = GetPieceType(pos->PieceOn(To(move)));
        if (capturedPiece == EMPTY) capturedPiece = KING; // Impossible to capture kings so we use it as "Empty" slot to save space
        return table[PieceTo(move)][capturedPiece];
    };

    inline void clear() {
        std::memset(table, 0, sizeof(table));
    };

    void update(const Position *pos, const Move move, const int16_t bonus);
    int16_t getScore(const Position *pos, const Move move) const;
};

// Continuation history is a history table for move pairs (i.e. a previous move and its continuation)
struct ContinuationHistoryTable {
    struct ContinuationHistoryEntry {
        int16_t factoriser;
    };

    // Indexed by [previous-piece][previous-to][current-piece][current-to]
    ContinuationHistoryEntry table[12 * 64][12 * 64];

    inline ContinuationHistoryEntry getEntry(const Position *pos, const Move prevMove, const Move currMove) const {
        return table[PieceTo(prevMove)][PieceTo(currMove)];
    };

    inline ContinuationHistoryEntry &getEntryRef(const Position *pos, const Move prevMove, const Move currMove) {
        return table[PieceTo(prevMove)][PieceTo(currMove)];
    };

    inline void clear() {
        std::memset(table, 0, sizeof(table));
    };

    void updateSingle(const Position *pos, const SearchStack *ss, const int offset, const Move move, const int16_t bonus);
    void update(const Position *pos, const SearchStack *ss, const Move move, const int16_t bonus);
    int16_t getScoreSingle(const Position *pos, const SearchStack *ss, const int offset, const Move move) const;
    int16_t getScore(const Position *pos, const SearchStack *ss, const Move move) const;
};

// Update all histories after a beta cutoff
void UpdateAllHistories(const Position *pos, const SearchStack *ss, SearchData *sd, const int depth, const Move bestMove, const MoveList &quietMoves, const MoveList &tacticalMoves);

// Get history score for a given move
int GetHistoryScore(const Position *pos, const SearchStack *ss, const SearchData *sd, const Move move);

// Clean all the history tables
void CleanHistories(SearchData* sd);
