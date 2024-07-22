#include "history.h"
#include <algorithm>
#include <cstring>
#include "position.h"
#include "move.h"
#include "search.h"

int16_t HistoryBonus(const int depth) {
    return std::min(histBonusQuadratic() * depth * depth + histBonusLinear() * depth + histBonusConst(), histBonusMax());
}

// Quiet history is a history table indexed by the side-to-move as well as a quiet move's [from-to].
void UpdateQuietHistory(const Position *pos, SearchData *sd, const Move move, const int16_t bonus) {
    // Scale the bonus so that the history, when updated, will be within [-quietHistMax(), quietHistMax()]
    const int scaledBonus = bonus - sd->quietHistory[pos->side][FromTo(move)] * std::abs(bonus) / quietHistMax();
    sd->quietHistory[pos->side][FromTo(move)] += bonus;
}

int16_t GetQuietHistoryScore(const Position *pos, const SearchData *sd, const Move move) {
    return sd->quietHistory[pos->side][FromTo(move)];
}

// Use this function to update all quiet histories
void UpdateAllHistories(const Position *pos, const SearchStack *ss, SearchData *sd, const int depth, const Move bestMove, const MoveList &quietMoves, const MoveList &tacticalMoves) {
    int16_t bonus = HistoryBonus(depth);
    if (isTactical(bestMove)) {
        ;
    }
    else {
        // Positively update the move that failed high
        UpdateQuietHistory(pos, sd, bestMove, bonus);

        // Penalise all quiets that failed to do so (they were ordered earlier but weren't as good)
        for (int i = 0; i < quietMoves.count; ++i) {
            Move quiet = quietMoves.moves[i].move;
            if (bestMove == quiet) continue;
            UpdateQuietHistory(pos, sd, quiet, -bonus);
        }
    }
}

int GetHistoryScore(const Position *pos, const SearchStack *ss, const SearchData *sd, const Move move) {
    if (isTactical(move)) {
        return 0;
    }
    else {
        return GetQuietHistoryScore(pos, sd, move);
    }
}

// Resets the history tables
void CleanHistories(SearchData *sd) {
    std::memset(sd->quietHistory, 0, sizeof(sd->quietHistory));
}
