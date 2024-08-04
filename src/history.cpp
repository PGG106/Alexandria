#include "history.h"
#include <algorithm>
#include <cstring>
#include "position.h"
#include "move.h"
#include "search.h"

int16_t HistoryBonus(const int depth) {
    return std::min(histBonusQuadratic() * depth * depth + histBonusLinear() * depth + histBonusConst(), histBonusMax());
}

void UpdateHistoryEntry(int16_t &entry, const int16_t bonus, const int16_t max) {
    const int scaledBonus = bonus - entry * std::abs(bonus) / max;
    entry += scaledBonus;
}

// Quiet history is a history table for quiet moves
void QuietHistoryTable::update(const Position *pos, const Move move, const int16_t bonus) {
    QuietHistoryEntry &entry = getEntryRef(pos, move);
    const int factoriserScale = quietHistFactoriserScale();
    const int bucketScale = 64 - factoriserScale;
    UpdateHistoryEntry(entry.factoriser, bonus * factoriserScale / 64, quietHistFactoriserMax());
    UpdateHistoryEntry(entry.bucketRef(pos, move), bonus * bucketScale / 64, quietHistBucketMax());
}

int16_t QuietHistoryTable::getScore(const Position *pos, const Move move) const {
    QuietHistoryEntry entry = getEntry(pos, move);
    return   entry.factoriser
           + entry.bucket(pos, move);
}

// Tactical history is a history table for tactical moves
void TacticalHistoryTable::update(const Position *pos, const Move move, int16_t bonus) {
    TacticalHistoryEntry &entry = getEntryRef(pos, move);
    UpdateHistoryEntry(entry.factoriser, bonus, tacticalHistMax());
}

int16_t TacticalHistoryTable::getScore(const Position *pos, const Move move) const {
    TacticalHistoryEntry entry = getEntry(pos, move);
    return entry.factoriser;
}

// Continuation history is a history table for move pairs (i.e. a previous move and its continuation)
void ContinuationHistoryTable::updateSingle(const Position *pos, const SearchStack *ss, const int offset, const Move move, const int16_t bonus) {
    ContinuationHistoryEntry &entry = getEntryRef(pos, (ss - offset)->move, move);
    UpdateHistoryEntry(entry.factoriser, bonus, continuationHistMax());
}

void ContinuationHistoryTable::update(const Position *pos, const SearchStack *ss, const Move move, const int16_t bonus) {
    updateSingle(pos, ss, 1, move, bonus);
    updateSingle(pos, ss, 2, move, bonus);
}

int16_t ContinuationHistoryTable::getScoreSingle(const Position *pos, const SearchStack *ss, const int offset, const Move move) const {
    ContinuationHistoryEntry entry = getEntry(pos, (ss - offset)->move, move);
    return entry.factoriser;
}

int16_t ContinuationHistoryTable::getScore(const Position *pos, const SearchStack *ss, const Move move) const {
    return   getScoreSingle(pos, ss, 1, move)
           + getScoreSingle(pos, ss, 2, move);
}

// Use this function to update all quiet histories
void UpdateAllHistories(const Position *pos, const SearchStack *ss, SearchData *sd, const int depth, const Move bestMove, const MoveList &quietMoves, const MoveList &tacticalMoves) {
    int16_t bonus = HistoryBonus(depth);
    if (isTactical(bestMove)) {
        // Positively update the move that failed high
        sd->tacticalHistory.update(pos, bestMove, bonus);
    }
    else {
        // Positively update the move that failed high
        sd->quietHistory.update(pos, bestMove, bonus);
        sd->continuationHistory.update(pos, ss, bestMove, bonus);

        // Penalise all quiets that failed to do so (they were ordered earlier but weren't as good)
        for (int i = 0; i < quietMoves.count; ++i) {
            Move quiet = quietMoves.moves[i].move;
            if (bestMove == quiet) continue;
            sd->quietHistory.update(pos, quiet, -bonus);
            sd->continuationHistory.update(pos, ss, quiet, -bonus);
        }
    }

    // Penalise all tactical moves that were searched first but didn't fail high (even if the best move was quiet)
    for (int i = 0; i < tacticalMoves.count; ++i) {
        Move tactical = tacticalMoves.moves[i].move;
        if (bestMove == tactical) continue;
        sd->tacticalHistory.update(pos, tactical, -bonus);
    }
}

int GetHistoryScore(const Position *pos, const SearchStack *ss, const SearchData *sd, const Move move) {
    if (isTactical(move)) {
        return sd->tacticalHistory.getScore(pos, move);
    }
    else {
        return   sd->quietHistory.getScore(pos, move)
               + sd->continuationHistory.getScore(pos, ss, move);
    }
}

// Resets the history tables
void CleanHistories(SearchData *sd) {
    sd->quietHistory.clear();
    sd->tacticalHistory.clear();
    sd->continuationHistory.clear();
}
