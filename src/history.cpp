#include "history.h"
#include <cstring>
#include "board.h"
#include "move.h"
#include "search.h"

/* History updating works in the same way for all histories, we have 3 methods:
updateScore: this updates the score of a specific entry of <History-name> type
UpdateHistories : this performs a general update of all the heuristics, giving the bonus to the best move and a malus to all the other
GetScore: this is simply a getter for a specific entry of the history table
*/

void updateHHScore(const S_Board* pos, Search_data* sd, int move, int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    const int scaledBonus = bonus - GetHHScore(pos, sd, move) * std::abs(bonus) / 32768;
    // Update move score
    sd->searchHistory[pos->side][From(move)]
        [To(move)] += scaledBonus;
}

void updateCHScore(Search_data* sd, const Search_stack* ss, const int move, const int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    const int scaledBonus = bonus - GetCHScore(sd, ss, move) * std::abs(bonus) / 32768;
    // Update move score
    if (ss->ply > 0) {
        sd->cont_hist[Piece((ss - 1)->move)][To((ss - 1)->move)]
            [Piece(move)][To(move)] += scaledBonus;
        // Score followup
        if (ss->ply > 1) {
            sd->cont_hist[Piece((ss - 2)->move)][To((ss - 2)->move)]
                [Piece(move)][To(move)] += scaledBonus;

            if (ss->ply > 3) {
                sd->cont_hist[Piece((ss - 4)->move)][To((ss - 4)->move)]
                    [Piece(move)][To(move)] += scaledBonus;
            }
        }
    }
}

void updateCapthistScore(const S_Board* pos, Search_data* sd, int move, int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    const int scaledBonus = bonus - GetCapthistScore(pos, sd, move) * std::abs(bonus) / 32768;
    int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
    // If we captured an empty piece this means the move is a promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
    if (capturedPiece == EMPTY) capturedPiece = PAWN;
    // Update move score
    sd->captHist[Piece(move)][To(move)][capturedPiece] += scaledBonus;
}

// Update all histories
void UpdateHistories(const S_Board* pos, Search_data* sd, Search_stack* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves, const S_MOVELIST* noisy_moves) {
    // define the history bonus
    const int bonus = std::min(16 * (depth + 1) * (depth + 1), 1200);
    if (!isTactical(bestmove))
    {
        // increase bestmove HH and CH score
        updateHHScore(pos, sd, bestmove, bonus);
        updateCHScore(sd, ss, bestmove, bonus);
        // Loop through all the quiet moves
        for (int i = 0; i < quiet_moves->count; i++) {
            // For all the quiets moves that didn't cause a cut-off decrease the HH score
            const int move = quiet_moves->moves[i].move;
            if (move == bestmove) continue;
            updateHHScore(pos, sd, move, -bonus);
            updateCHScore(sd, ss, move, -bonus);
        }
    }
    else {
        // increase the bestmove Capthist score
        updateCapthistScore(pos, sd, bestmove, bonus);
    }
    // For all the noisy moves that didn't cause a cut-off, even is the bestmove wasn't a noisy move, decrease the capthist score
    for (int i = 0; i < noisy_moves->count; i++) {
        const int move = noisy_moves->moves[i].move;
        if (move == bestmove) continue;
        updateCapthistScore(pos, sd, move, -bonus);
    }
}

// Returns the history score of a move
int GetHHScore(const S_Board* pos, const Search_data* sd, const int move) {
    return sd->searchHistory[pos->side][From(move)][To(move)];
}

// Returns the history score of a move
int GetCHScore(const Search_data* sd, const Search_stack* ss, const int move) {
    int score = 0;
    const int previousMove = (ss - 1)->move;
    const int previousPreviousMove = (ss - 2)->move;
    const int previousPreviousPreviousPreviousMove = (ss - 4)->move;
    if (previousMove)
        score += sd->cont_hist[Piece(previousMove)][To(previousMove)][Piece(move)][To(move)];
    if (previousPreviousMove)
        score += sd->cont_hist[Piece(previousPreviousMove)][To(previousPreviousMove)][Piece(move)][To(move)];
    if (previousPreviousPreviousPreviousMove)
        score += sd->cont_hist[Piece(previousPreviousPreviousPreviousMove)][To(previousPreviousPreviousPreviousMove)][Piece(move)][To(move)];
    return score;
}

// Returns the history score of a move
int GetCapthistScore(const S_Board* pos, const Search_data* sd, const int move) {
    int capturedPiece = isEnpassant(move) ? PAWN : GetPieceType(pos->PieceOn(To(move)));
    // If we captured an empty piece this means the move is a non capturing promotion, we can pretend we captured a pawn to use a slot of the table that would've otherwise went unused (you can't capture pawns on the 1st/8th rank)
    if (capturedPiece == EMPTY) capturedPiece = PAWN;
    return sd->captHist[Piece(move)][To(move)][capturedPiece];
}

int GetHistoryScore(const S_Board* pos, const Search_data* sd, const int move, const Search_stack* ss) {
    if (!isTactical(move))
        return GetHHScore(pos, sd, move) + 2 * GetCHScore(sd, ss, move);
    else
        return GetCapthistScore(pos, sd, move);
}

// Resets the history tables
void CleanHistories(Search_data* sd) {
    std::memset(sd->searchHistory, 0, sizeof(sd->searchHistory));
    std::memset(sd->cont_hist, 0, sizeof(sd->cont_hist));
    std::memset(sd->captHist, 0, sizeof(sd->captHist));
}
