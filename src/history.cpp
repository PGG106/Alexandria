#include "history.h"
#include <cstring>
#include "board.h"
#include "move.h"
#include "search.h"

/* History updating works in the same way for all histories, we have 3 methods:
updateScore: this updates the score of a specific entry of <History-name> type
Update<History-name> : this performs a general update of the heuristic, giving the bonus to the best move and a malus to all the other
GetScore: this is simply a getter for a specific entry of the history table
*/

void updateHHScore(const S_Board* pos, Search_data* sd, int move, int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    int scaled_bonus = bonus - GetHHScore(pos, sd, move) * std::abs(bonus) / 32768;
    // Update move score
    sd->searchHistory[pos->side][From(move)]
        [To(move)] += scaled_bonus;
}

void updateCHScore(Search_data* sd, const Search_stack* ss, const int move, const int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    int scaled_bonus = bonus - GetCHScore(sd, ss, move) * std::abs(bonus) / 32768;
    // Update move score
    if (ss->ply > 0) {
        sd->cont_hist[Piece((ss - 1)->move)][To((ss - 1)->move)]
            [Piece(move)][To(move)] += scaled_bonus;
        // Score followup
        if (ss->ply > 1) {
            sd->cont_hist[Piece((ss - 2)->move)][To((ss - 2)->move)]
                [Piece(move)][To(move)] += scaled_bonus;

            if (ss->ply > 3) {
                sd->cont_hist[Piece((ss - 4)->move)][To((ss - 4)->move)]
                    [Piece(move)][To(move)] += scaled_bonus;
            }
        }
    }
}

void updateCapthistScore(const S_Board* pos, Search_data* sd, int move, int bonus) {
    // Scale bonus to fix it in a [-32768;32768] range
    int scaled_bonus = bonus - GetCapthistScore(pos, sd, move) * std::abs(bonus) / 32768;
    int captured_piece = isEnpassant(move) ? PAWN : pos->PieceOn(To(move));
    if (captured_piece == EMPTY)
        captured_piece = PAWN;
    int captured_piece_type = GetPieceType(captured_piece);
    // Update move score
    sd->captHist[Piece(move)][To(move)][captured_piece_type] += scaled_bonus;
}

// Update all histories
void UpdateHistories(const S_Board* pos, Search_data* sd, Search_stack* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves, const S_MOVELIST* noisy_moves) {
    // define the history bonus
    int bonus = std::min(16 * depth * depth, 1200);
    if (IsQuiet(bestmove))
    {
        // increase bestmove HH and CH score
        updateHHScore(pos, sd, bestmove, bonus);
        updateCHScore(sd, ss, bestmove, bonus);
        // Loop through all the quiet moves
        for (int i = 0; i < quiet_moves->count; i++) {
            // For all the quiets moves that didn't cause a cut-off decrease the HH score
            int move = quiet_moves->moves[i].move;
            if (move == bestmove) continue;
            updateHHScore(pos, sd, move, -bonus);
            updateCHScore(sd, ss, move, -bonus);
        }
    }
    else {
        updateCapthistScore(pos, sd, bestmove, bonus);
    }
    // For all the noisy moves that didn't cause a cut-off, even is the bestmove wasn't a noisy move, decrease the capthist score
    for (int i = 0; i < noisy_moves->count; i++) {
        int move = noisy_moves->moves[i].move;
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
    int previous_move = (ss - 1)->move;
    int previous_previous_move = (ss - 2)->move;
    int previous_previous_previous_previous_move = (ss - 4)->move;
    if (previous_move)
        score += sd->cont_hist[Piece(previous_move)][To(previous_move)][Piece(move)][To(move)];
    if (previous_previous_move)
        score += sd->cont_hist[Piece(previous_previous_move)][To(previous_previous_move)][Piece(move)][To(move)];
    if (previous_previous_previous_previous_move)
        score += sd->cont_hist[Piece(previous_previous_previous_previous_move)][To(previous_previous_previous_previous_move)][Piece(move)][To(move)];
    return score;
}

// Returns the history score of a move
int GetCapthistScore(const S_Board* pos, const Search_data* sd, const int move) {
    int captured_piece = isEnpassant(move) ? PAWN : pos->PieceOn(To(move));
    if (captured_piece == EMPTY)
        captured_piece = PAWN;
    int captured_piece_type = GetPieceType(captured_piece);
    return sd->captHist[Piece(move)][To(move)][captured_piece_type];
}

int GetHistoryScore(const S_Board* pos, const Search_data* sd, const int move, const Search_stack* ss) {
    return GetHHScore(pos, sd, move) + 2 * GetCHScore(sd, ss, move);
}

// Resets the history tables
void CleanHistories(Search_data* ss) {
    std::memset(ss->searchHistory, 0, sizeof(ss->searchHistory));
    std::memset(ss->cont_hist, 0, sizeof(ss->cont_hist));
    std::memset(ss->captHist, 0, sizeof(ss->captHist));
}
