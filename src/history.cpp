#include "history.h"
#include <algorithm>
#include <cstring>
#include "position.h"
#include "move.h"
#include "search.h"

// Quiet history is a history table for quiet moves
void QuietHistoryTable::update(const Position *pos, const Move move, const int16_t bonus) {
    QuietHistoryEntry &entry = getEntryRef(pos, move);
    const int factoriserScale = quietHistFactoriserScale();
    const int bucketScale = 64 - factoriserScale;
    UpdateHistoryEntry(entry.factoriser, bonus * factoriserScale / 64, quietHistFactoriserMax());
    UpdateHistoryEntry(entry.bucketRef(pos, move), bonus * bucketScale / 64, quietHistBucketMax());
}

int QuietHistoryTable::getScore(const Position *pos, const Move move) const {
    QuietHistoryEntry entry = getEntry(pos, move);
    return   entry.factoriser
           + entry.bucket(pos, move);
}

// Tactical history is a history table for tactical moves
void TacticalHistoryTable::update(const Position *pos, const Move move, int16_t bonus) {
    TacticalHistoryEntry &entry = getEntryRef(pos, move);
    UpdateHistoryEntry(entry.factoriser, bonus, tacticalHistMax());
}

int TacticalHistoryTable::getScore(const Position *pos, const Move move) const {
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
    updateSingle(pos, ss, 4, move, bonus);
}

int ContinuationHistoryTable::getScoreSingle(const Position *pos, const SearchStack *ss, const int offset, const Move move) const {
    ContinuationHistoryEntry entry = getEntry(pos, (ss - offset)->move, move);
    return entry.factoriser;
}

int ContinuationHistoryTable::getScore(const Position *pos, const SearchStack *ss, const Move move) const {
    return   getScoreSingle(pos, ss, 1, move)
           + getScoreSingle(pos, ss, 2, move)
           + getScoreSingle(pos, ss, 4, move);
}

// CorrectionHistoryTable is a history table indexed by [side-to-move][pawn-key-index]. It is used to correct eval
void CorrectionHistoryTable::update(const Position *pos, const Move bestMove, const int depth, const uint8_t bound, const int bestScore, const int rawEval) {
    if (pos->getCheckers()) return; // Don't update correction history if in check
    if (bestMove != NOMOVE && isTactical(bestMove)) return; // Don't update correction history for tactical best moves
    if (bound == HFUPPER && rawEval < bestScore) return; // Don't update correction history if the raw eval is a better upper bound
    if (bound == HFLOWER && rawEval > bestScore) return; // Don't update correction history if the raw eval is a better lower bound

    int16_t &entry = getEntryRef(pos);
    const int scaledDiff = std::clamp((bestScore - rawEval) * Grain, -corrHistMaxAdjust(), corrHistMaxAdjust());
    const int newWeight = weight(depth);
    assert(newWeight <= MaxWeight);

    entry = (entry * (MaxWeight - newWeight) + scaledDiff * newWeight) / MaxWeight;
}

int16_t CorrectionHistoryTable::adjust(const Position *pos, const int eval) const {
    int16_t entry = getEntry(pos);
    return eval + entry / Grain;
}

// Use this function to update all quiet histories
void UpdateAllHistories(const Position *pos, const SearchStack *ss, SearchData *sd, const int depth, const Move bestMove,
                        const SearchedMoveList &quietMoves, const SearchedMoveList &tacticalMoves, const int eval, const int alpha, const int beta) {

    auto getBonus = [&](const SearchedMove move) {
        int bonusDepth = depth;

        // Increase bonus if our eval suggested we were failing low (result was against expectations)
        if (eval <= alpha) bonusDepth += 1;

        // Decrease bonus if our eval suggested we were failing high (result was expected outcome)
        if (eval >= beta) bonusDepth -= 1;

        // If a full window search was performed, give an x1 multiplier.
        // Then, if a full depth zero-window search was performed, give an x2 multiplier.
        // In the base case, give an x3 multiplier.
        const int bonusMultiplier = move.bonusScale();

        return HistoryBonus(bonusDepth) * bonusMultiplier;
    };

    auto getMalus = [&](const SearchedMove move) {
        int malusDepth = depth;

        // Decrease malus if our eval suggested we were failing low (result was expected outcome for this move)
        if (eval <= alpha) malusDepth -= 1;

        // Increase malus if our eval suggested we were failing high (result was against expectations for this move)
        if (eval >= beta) malusDepth += 1;

        // If a full window search was performed, give an x3 multiplier.
        // Then, if a full depth zero-window search was performed, give an x2 multiplier.
        // In the base case, give an x1 multiplier.
        const int malusMultiplier = move.malusScale();

        return -HistoryBonus(malusDepth) * malusMultiplier;
    };

    if (!isTactical(bestMove)) {
        // Positively update best move
        // Penalise all quiets that failed to do so (they were ordered earlier but weren't as good)
        for (int i = 0; i < quietMoves.count; ++i) {
            SearchedMove quietData = quietMoves.moves[i];
            Move quiet = quietData.move;
            int update = quiet == bestMove ? getBonus(quietData) : getMalus(quietData);
            sd->quietHistory.update(pos, quiet, update);
            sd->continuationHistory.update(pos, ss, quiet, update);
        }
    }

    // Positively update best move
    // Penalise all tactical moves that were searched first but didn't fail high (even if the best move was quiet)
    for (int i = 0; i < tacticalMoves.count; ++i) {
        SearchedMove tacticalData = tacticalMoves.moves[i];
        Move tactical = tacticalData.move;
        int update = tactical == bestMove ? getBonus(tacticalData) : getMalus(tacticalData);
        sd->tacticalHistory.update(pos, tactical, update);
        sd->continuationHistory.update(pos, ss, tactical, update);
    }
}

int GetHistoryScore(const Position *pos, const SearchStack *ss, const SearchData *sd, const Move move) {
    if (isTactical(move)) {
        return   sd->tacticalHistory.getScore(pos, move)
               + sd->continuationHistory.getScore(pos, ss, move);
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
    sd->correctionHistory.clear();
}