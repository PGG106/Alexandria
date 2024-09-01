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

inline int16_t HistoryBonus(const int depth) {
    return std::min(  histBonusQuadratic() * depth * depth
                    + histBonusLinear() * depth
                    + histBonusConst(), histBonusMax());
}

inline void UpdateHistoryEntry(int16_t &entry, const int16_t bonus, const int16_t max) {
    const int clampedBonus = std::clamp<int16_t>(bonus, -max, max);
    const int scaledBonus  = clampedBonus - entry * std::abs(clampedBonus) / max;
    entry += scaledBonus;
}

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
    int getScore(const Position *pos, const Move move) const;
};

// Tactical history is a history table for tactical moves
struct TacticalHistoryTable {
    struct TacticalHistoryEntry {
        int16_t factoriser;
    };

    // Indexed by [moved-piece][to-square][captured-piece]
    TacticalHistoryEntry table[12 * 64][6];

    inline TacticalHistoryEntry getEntry(const Position *pos, const Move move) const {
        int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
        if (capturedPiece == EMPTY) capturedPiece = KING; // Impossible to capture kings so we use it as "Empty" slot to save space
        return table[PieceTo(move)][capturedPiece];
    };

    inline TacticalHistoryEntry &getEntryRef(const Position *pos, const Move move) {
        int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
        if (capturedPiece == EMPTY) capturedPiece = KING; // Impossible to capture kings so we use it as "Empty" slot to save space
        return table[PieceTo(move)][capturedPiece];
    };

    inline void clear() {
        std::memset(table, 0, sizeof(table));
    };

    void update(const Position *pos, const Move move, const int16_t bonus);
    int getScore(const Position *pos, const Move move) const;
};

// Continuation history is a history table for move pairs (i.e. a previous move and its continuation)
struct ContinuationHistoryTable {
    struct ContinuationHistoryEntry {
        int16_t factoriser;
    };

    // Indexed by [previous-piece-to][current-piece-to][current-captured-piece]
    ContinuationHistoryEntry table[12 * 64][12 * 64][6];

    inline ContinuationHistoryEntry getEntry(const Position *pos, const Move prevMove, const Move currMove) const {
        int capturedPiece = isEnpassant(currMove) ? PAWN : GetPieceType(pos->PieceOn(To(currMove)));
        if (capturedPiece == EMPTY) capturedPiece = KING; // Impossible to capture kings so we use it as "Empty" slot to save space
        return table[PieceTo(prevMove)][PieceTo(currMove)][capturedPiece];
    };

    inline ContinuationHistoryEntry &getEntryRef(const Position *pos, const Move prevMove, const Move currMove) {
        int capturedPiece = isEnpassant(currMove) ? PAWN : GetPieceType(pos->PieceOn(To(currMove)));
        if (capturedPiece == EMPTY) capturedPiece = KING; // Impossible to capture kings so we use it as "Empty" slot to save space
        return table[PieceTo(prevMove)][PieceTo(currMove)][capturedPiece];
    };

    inline void clear() {
        std::memset(table, 0, sizeof(table));
    };

    void updateSingle(const Position *pos, const SearchStack *ss, const int offset, const Move move, const int16_t bonus);
    void update(const Position *pos, const SearchStack *ss, const Move move, const int16_t bonus);
    int getScoreSingle(const Position *pos, const SearchStack *ss, const int offset, const Move move) const;
    int getScore(const Position *pos, const SearchStack *ss, const Move move) const;
};

// CorrectionHistoryTable is a history table indexed by [pawn-key-index]. It is used to correct eval
struct CorrectionHistoryTable {
    static constexpr int Size = 1 << 16;
    static constexpr int Mask = Size - 1;
    static_assert((Size & Mask) == 0);

    static constexpr int Grain = 256;
    static constexpr int MaxWeight = 1024;

    int16_t table[2][Size];

    inline int weight(const int depth) const {
        return std::min(  corrHistWeightQuadratic() * depth * depth
                        + corrHistWeightLinear() * depth
                        + corrHistWeightConst(), corrHistWeightMax());
    };

    inline int16_t &getEntryRef(const Position *pos) {
        return table[pos->side][pos->pawnKey & Mask];
    }

    inline int16_t getEntry(const Position *pos) const {
        return table[pos->side][pos->pawnKey & Mask];
    };

    inline void clear() {
        std::memset(table, 0, sizeof(table));
    };

    void update(const Position *pos, const Move bestMove, const int depth, const uint8_t bound, const int bestScore, const int rawEval);
    int16_t adjust(const Position *pos, const int eval) const;
};

// Update all histories after a beta cutoff
void UpdateAllHistories(const Position *pos, const SearchStack *ss, SearchData *sd, const int bonusDepth, const int malusDepth, const Move bestMove, const MoveList &quietMoves, const MoveList &tacticalMoves);

// Get history score for a given move
int GetHistoryScore(const Position *pos, const SearchStack *ss, const SearchData *sd, const Move move);

// Clean all the history tables
void CleanHistories(SearchData* sd);