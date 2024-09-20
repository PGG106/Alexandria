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
    return std::min(16 * depth * depth + 32 * depth + 16, 1200);
}

void updateHHScore(const Position* pos, SearchData* sd, const Move move, int bonus) {
    // Scale bonus to fix it in a [-HH_MAX;HH_MAX] range
    const int scaledBonus = bonus - GetHHScore(pos, sd, move) * std::abs(bonus) / HH_MAX;
    // Update move score
    sd->searchHistory[pos->side][FromTo(move)] += scaledBonus;
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
            updateHHScore(pos, sd, move, -bonus);
            updateCHScore(ss, move, -bonus);
            if (rootNode)
              updateRHScore(pos, sd, move, -bonus);
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
        updateCapthistScore(pos, sd, move, -bonus);
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

void updateCorrHistScore(const Position *pos, SearchData *sd, const int depth, const int diff) {
    int &entry = sd->corrHist[pos->side][pos->pawnKey % CORRHIST_SIZE];
    const int scaledDiff = diff * CORRHIST_GRAIN;
    const int newWeight = std::min(depth * depth + 2 * depth + 1, 128);
    assert(newWeight <= CORRHIST_WEIGHT_SCALE);

    entry = (entry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    entry = std::clamp(entry, -CORRHIST_MAX, CORRHIST_MAX);
}

int adjustEvalWithCorrHist(const Position *pos, const SearchData *sd, const int rawEval) {
    const int &entry = sd->corrHist[pos->side][pos->pawnKey % CORRHIST_SIZE];
    return std::clamp(rawEval + entry / CORRHIST_GRAIN, -MATE_FOUND + 1, MATE_FOUND - 1);
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
    std::memset(sd->corrHist, 0, sizeof(sd->corrHist));
}
