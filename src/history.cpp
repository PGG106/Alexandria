#include "history.h"
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
    return std::min(16 * (depth + 1) * (depth + 1), 1200);
}

void updateHHScore(const Position* pos, SearchData* sd, int move, int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    const int scaledBonus = bonus - GetHHScore(pos, sd, move) * std::abs(bonus) / 32768;
    // Update move score
    sd->searchHistory[pos->side][From(move)][To(move)] += scaledBonus;
}

void updateCHScore(SearchData* sd, const SearchStack* ss, const int move, const int bonus) {
    // Average out the bonus across the 3 conthist entries
    const int scaledBonus = bonus - GetCHScore(sd, ss, move) * std::abs(bonus) / 32768;
    // Update move score
    updateSingleCHScore(sd, ss, move, scaledBonus, 1);
    updateSingleCHScore(sd, ss, move, scaledBonus, 2);
    updateSingleCHScore(sd, ss, move, scaledBonus, 4);
}

void updateSingleCHScore(SearchData* sd, const SearchStack* ss, const int move, const int bonus, const int offset) {
    if (ss->ply >= offset) {
        const int previousMove = (ss - offset)->move;
        const int scaledBonus = bonus - GetSingleCHScore(sd, ss, move, offset) * std::abs(bonus) / 65536;
        sd->contHist[Piece(previousMove)][To(previousMove)][Piece(move)][To(move)] += scaledBonus;
    }
}

void updateCapthistScore(const Position* pos, SearchData* sd, int move, int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    const int scaledBonus = bonus - GetCapthistScore(pos, sd, move) * std::abs(bonus) / 32768;
    int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
    // If we captured an empty piece this means the move is a promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
    if (capturedPiece == EMPTY) capturedPiece = PAWN;
    // Update move score
    sd->captHist[Piece(move)][To(move)][capturedPiece] += scaledBonus;
}

// Update all histories
void UpdateHistories(const Position* pos, SearchData* sd, SearchStack* ss, const int depth, const int bestMove, const MoveList* quietMoves, const MoveList* noisyMoves) {
    const int bonus = history_bonus(depth);
    if (!isTactical(bestMove))
    {
        // increase bestMove HH and CH score
        updateHHScore(pos, sd, bestMove, bonus);
        updateCHScore(sd, ss, bestMove, bonus);
        // Loop through all the quiet moves
        for (int i = 0; i < quietMoves->count; i++) {
            // For all the quiets moves that didn't cause a cut-off decrease the HH score
            const int move = quietMoves->moves[i].move;
            if (move == bestMove) continue;
            updateHHScore(pos, sd, move, -bonus);
            updateCHScore(sd, ss, move, -bonus);
        }
    }
    else {
        // increase the bestMove Capthist score
        updateCapthistScore(pos, sd, bestMove, bonus);
    }
    // For all the noisy moves that didn't cause a cut-off, even is the bestMove wasn't a noisy move, decrease the capthist score
    for (int i = 0; i < noisyMoves->count; i++) {
        const int move = noisyMoves->moves[i].move;
        if (move == bestMove) continue;
        updateCapthistScore(pos, sd, move, -bonus);
    }
}

// Returns the history score of a move
int GetHHScore(const Position* pos, const SearchData* sd, const int move) {
    return sd->searchHistory[pos->side][From(move)][To(move)];
}

// Returns the history score of a move
int GetCHScore(const SearchData* sd, const SearchStack* ss, const int move) {
    return   GetSingleCHScore(sd, ss, move, 1)
           + GetSingleCHScore(sd, ss, move, 2)
           + GetSingleCHScore(sd, ss, move, 4);
}

int GetSingleCHScore(const SearchData* sd, const SearchStack* ss, const int move, const int offset) {
    const int previousMove = (ss - offset)->move;
    return previousMove ? sd->contHist[Piece(previousMove)][To(previousMove)][Piece(move)][To(move)] : 0;
}

// Returns the history score of a move
int GetCapthistScore(const Position* pos, const SearchData* sd, const int move) {
    int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
    // If we captured an empty piece this means the move is a non capturing promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
    if (capturedPiece == EMPTY) capturedPiece = PAWN;
    return sd->captHist[Piece(move)][To(move)][capturedPiece];
}

int GetHistoryScore(const Position* pos, const SearchData* sd, const int move, const SearchStack* ss) {
    if (!isTactical(move))
        return GetHHScore(pos, sd, move) + 2 * GetCHScore(sd, ss, move);
    else
        return GetCapthistScore(pos, sd, move);
}

// Resets the history tables
void CleanHistories(SearchData* sd) {
    std::memset(sd->searchHistory, 0, sizeof(sd->searchHistory));
    std::memset(sd->contHist, 0, sizeof(sd->contHist));
    std::memset(sd->captHist, 0, sizeof(sd->captHist));
}
