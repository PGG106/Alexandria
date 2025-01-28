#include "history.h"
#include <algorithm>
#include <cstring>
#include "position.h"
#include "move.h"
#include "search.h"

/* History updating works in the same way for all histories, we have 3 methods:
updateScore: this updates the score of a specific entry of <History-name> type
UpdateHistories : this performs a general update of all the heuristics, giving the bonus to the best move and a malus to all the other
GetScore: this is simply a getter for a specific entry of the history table
*/

int history_bonus(const int depth) {
    return std::min(300 * depth + 111, historyBonusMax());
}

int history_malus(const int depth) {
    return std::min(350 * depth - 147, historyMalusMax());
}

void updateHHScore(const Position* pos, SearchData* sd, const Move move, int bonus) {
    // Scale bonus to fix it in a [-HH_MAX;HH_MAX] range
    const int scaledBonus = bonus - GetHHScore(pos, sd, move) * std::abs(bonus) / HH_MAX;
    // Update move score
    sd->searchHistory[pos->side][FromTo(move)] += scaledBonus;
}

void updateOppHHScore(const Position* pos, SearchData* sd, const Move move, int bonus) {
    // Scale bonus to fix it in a [-HH_MAX;HH_MAX] range
    const int scaledBonus = bonus - sd->searchHistory[pos->side ^ 1][FromTo(move)] * std::abs(bonus) / HH_MAX;
    // Update move score
    sd->searchHistory[pos->side ^ 1][FromTo(move)] += scaledBonus;
}

void updateRHScore(const Position *pos, SearchData *sd, const Move move, int bonus) {
    // Scale bonus to fix it in a [-RH_MAX;RH_MAX] range
    const int scaledBonus = bonus - GetRHScore(pos, sd, move) * std::abs(bonus) / RH_MAX;
    // Update move score
    sd->rootHistory[pos->side][FromTo(move)] += scaledBonus;
}

void updateCHScore(SearchStack* ss, const Move move, const int bonus) {
    // Update move score
    updateSingleCHScore(ss, move, bonus, 1);
    updateSingleCHScore(ss, move, bonus, 2);
    updateSingleCHScore(ss, move, bonus, 4);
}

void updateSingleCHScore(SearchStack* ss, const Move move, const int bonus, const int offset) {
    if ((ss - offset)->move) {
        // Scale bonus to fix it in a [-CH_MAX;CH_MAX] range
        const int scaledBonus = bonus - GetSingleCHScore(ss, move, offset) * std::abs(bonus) / CH_MAX;
        (*((ss - offset)->contHistEntry))[PieceTo(move)] += scaledBonus;
    }
}

void updateCapthistScore(const Position* pos, SearchData* sd, const Move move, int bonus) {
    // Scale bonus to fix it in a [-CAPTHIST_MAX;CAPTHIST_MAX] range
    const int scaledBonus = bonus - GetCapthistScore(pos, sd, move) * std::abs(bonus) / CAPTHIST_MAX;
    int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
    // If we captured an empty piece this means the move is a promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
    if (capturedPiece == EMPTY) capturedPiece = PAWN;
    // Update move score
    sd->captHist[PieceTo(move)][capturedPiece] += scaledBonus;
}

// Update all histories
void UpdateHistories(const Position* pos, SearchData* sd, SearchStack* ss, const int depth, const Move bestMove, const MoveList* quietMoves, const MoveList* noisyMoves, const bool rootNode) {
    const int bonus = history_bonus(depth);
    const int malus = history_malus(depth);
    if (!isTactical(bestMove))
    {
        // increase bestMove HH and CH score
        updateHHScore(pos, sd, bestMove, bonus);
        updateCHScore(ss, bestMove, bonus);
        if (rootNode)
            updateRHScore(pos, sd, bestMove, bonus);
        // Loop through all the quiet moves
        for (int i = 0; i < quietMoves->count; i++) {
            // For all the quiets moves that didn't cause a cut-off decrease the HH score
            const Move move = quietMoves->moves[i].move;
            if (move == bestMove) continue;
            updateHHScore(pos, sd, move, -malus);
            updateCHScore(ss, move, -malus);
            if (rootNode)
              updateRHScore(pos, sd, move, -malus);
        }
    }
    else {
        // increase the bestMove Capthist score
        updateCapthistScore(pos, sd, bestMove, bonus);
    }
    // For all the noisy moves that didn't cause a cut-off, even is the bestMove wasn't a noisy move, decrease the capthist score
    for (int i = 0; i < noisyMoves->count; i++) {
        const Move move = noisyMoves->moves[i].move;
        if (move == bestMove) continue;
        updateCapthistScore(pos, sd, move, -malus);
    }
}

// Returns the history score of a move
int GetHHScore(const Position* pos, const SearchData* sd, const Move move) {
    return sd->searchHistory[pos->side][FromTo(move)];
}

int GetRHScore(const Position *pos, const SearchData *sd, const Move move) {
    return sd->rootHistory[pos->side][FromTo(move)];
}

// Returns the history score of a move
int GetCHScore(const SearchStack* ss, const Move move) {
    return   GetSingleCHScore(ss, move, 1)
           + GetSingleCHScore(ss, move, 2)
           + GetSingleCHScore(ss, move, 4);
}

int GetSingleCHScore(const SearchStack* ss, const Move move, const int offset) {
    return (ss - offset)->move ? (*((ss - offset)->contHistEntry))[PieceTo(move)]
                               : 0;
}

// Returns the history score of a move
int GetCapthistScore(const Position* pos, const SearchData* sd, const Move move) {
    int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
    // If we captured an empty piece this means the move is a non capturing promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
    if (capturedPiece == EMPTY) capturedPiece = PAWN;
    return sd->captHist[PieceTo(move)][capturedPiece];
}

void updateSingleCorrHistScore(int &entry, const int scaledDiff, const int newWeight) {
    entry = (entry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    entry = std::clamp(entry, -CORRHIST_MAX, CORRHIST_MAX);
}

void updateCorrHistScore(const Position *pos, SearchData *sd, const SearchStack* ss, const int depth, const int diff) {
    const int scaledDiff = diff * CORRHIST_GRAIN;
    const int newWeight = std::min(depth * depth + 2 * depth + 1, 128);
    assert(newWeight <= CORRHIST_WEIGHT_SCALE);

    updateSingleCorrHistScore(sd->pawnCorrHist[pos->side][pos->pawnKey % CORRHIST_SIZE], scaledDiff, newWeight);
    updateSingleCorrHistScore(sd->whiteNonPawnCorrHist[pos->side][pos->whiteNonPawnKey % CORRHIST_SIZE], scaledDiff, newWeight);
    updateSingleCorrHistScore(sd->blackNonPawnCorrHist[pos->side][pos->blackNonPawnKey % CORRHIST_SIZE], scaledDiff, newWeight);

    if ((ss - 1)->move && (ss - 2)->move)
        updateSingleCorrHistScore(sd->contCorrHist[pos->side][PieceTypeTo((ss - 1)->move)][PieceTypeTo((ss - 2)->move)], scaledDiff, newWeight);
}

int adjustEvalWithCorrHist(const Position *pos, const SearchData *sd, const SearchStack* ss, const int rawEval) {
    int adjustment = 0;

    adjustment += sd->pawnCorrHist[pos->side][pos->pawnKey % CORRHIST_SIZE];
    adjustment += sd->whiteNonPawnCorrHist[pos->side][pos->whiteNonPawnKey % CORRHIST_SIZE];
    adjustment += sd->blackNonPawnCorrHist[pos->side][pos->blackNonPawnKey % CORRHIST_SIZE];

    if ((ss - 1)->move && (ss - 2)->move)
        adjustment += sd->contCorrHist[pos->side][PieceTypeTo((ss - 1)->move)][PieceTypeTo((ss - 2)->move)];

    return std::clamp(rawEval + adjustment / CORRHIST_GRAIN, -MATE_FOUND + 1, MATE_FOUND - 1);
}

int GetHistoryScore(const Position* pos, const SearchData* sd, const Move move, const SearchStack* ss, const bool rootNode) {
    if (!isTactical(move))
        return GetHHScore(pos, sd, move) + GetCHScore(ss, move) + rootNode * 4 * GetRHScore(pos, sd, move);
    else
        return GetCapthistScore(pos, sd, move);
}

// Resets the history tables
void CleanHistories(SearchData* sd) {
    std::memset(sd->searchHistory, 0, sizeof(sd->searchHistory));
    std::memset(sd->rootHistory, 0, sizeof(sd->rootHistory));
    std::memset(sd->contHist, 0, sizeof(sd->contHist));
    std::memset(sd->captHist, 0, sizeof(sd->captHist));
    std::memset(sd->pawnCorrHist, 0, sizeof(sd->pawnCorrHist));
    std::memset(sd->whiteNonPawnCorrHist, 0, sizeof(sd->whiteNonPawnCorrHist));
    std::memset(sd->blackNonPawnCorrHist, 0, sizeof(sd->blackNonPawnCorrHist));
    std::memset(sd->contCorrHist, 0, sizeof(sd->contCorrHist));
}
