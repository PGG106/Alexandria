#include "history.h"
#include <cstring>
#include "board.h"
#include "move.h"
#include "search.h"

void updateHHScore(const S_Board* pos, Search_data* ss, int move, int bonus) {
    //Scale bonus to fix it in a [-32768;32768] range
    int scaled_bonus = bonus - GetHHScore(pos, ss, move) * std::abs(bonus) / 32768;
    //Update move score
    ss->searchHistory[pos->side][From(move)]
        [To(move)] += scaled_bonus;
}

void updateCHScore(const S_Board* pos, Search_data* sd, const Search_stack* ss, const int move, const int bonus) {
    //Scale bonus to fix it in a [-32768;32768] range
    int scaled_bonus = bonus - GetCHScore(sd, ss, move) * std::abs(bonus) / 32768;
    //Update move score
    if (ss->ply > 0) {
        sd->cont_hist[Piece((ss - 1)->move)][To((ss - 1)->move)]
            [Piece(move)][To(move)] += scaled_bonus;
        //Score followup
        if (ss->ply > 1) {
            sd->cont_hist[Piece((ss - 2)->move)][To((ss - 2)->move)]
                [Piece(move)][To(move)] += scaled_bonus;
        }
    }
}

//Update the history heuristics of all the quiet moves passed to the function
void UpdateHH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves) {
    //define the history bonus
    int bonus = std::min(16 * depth * depth, 1200);
    //increase bestmove HH score
    updateHHScore(pos, ss, bestmove, bonus);
    //Loop through all the quiet moves
    for (int i = 0; i < quiet_moves->count; i++) {
        //For all the quiets moves that didn't cause a cut-off decrease the HH score
        int move = quiet_moves->moves[i].move;
        if (move == bestmove) continue;
        updateHHScore(pos, ss, move, -bonus);
    }
}

//Update the history heuristics of all the quiet moves passed to the function
void UpdateCH(const S_Board* pos, Search_data* sd, const Search_stack* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves) {
    //define the conthist bonus
    int bonus = std::min(16 * depth * depth, 1200);
    //increase bestmove CH score
    updateCHScore(pos, sd, ss, bestmove, bonus);
    //Loop through all the quiet moves
    for (int i = 0; i < quiet_moves->count; i++) {
        //For all the quiets moves that didn't cause a cut-off decrease the CH score
        int move = quiet_moves->moves[i].move;
        if (move == bestmove) continue;
        updateCHScore(pos, sd, ss, move, -bonus);
    }
}

//Returns the history score of a move
int GetHHScore(const S_Board* pos, const Search_data* sd, const int move) {
    return sd->searchHistory[pos->side][From(move)][To(move)];
}

int GetHistoryScore(const S_Board* pos, const Search_data* sd, const int move, const Search_stack* ss) {
    return GetHHScore(pos, sd, move) + GetCHScore(sd, ss, move);
}

//Returns the history score of a move
int GetCHScore(const Search_data* sd, const Search_stack* ss, const int move) {
    int score = 0;
    int previous_move = (ss - 1)->move;
    int previous_previous_move = (ss - 2)->move;
    if (previous_move)
        score += sd->cont_hist[Piece(previous_move)][To(previous_move)][Piece(move)][To(move)];
    if (previous_previous_move)
        score += sd->cont_hist[Piece(previous_previous_move)][To(previous_previous_move)][Piece(move)][To(move)];
    return score;
}

//Resets the history table
void CleanHistories(Search_data* ss) {
    //For every piece [12] moved to every square [64] we reset the searchHistory value
    for (int index = 0; index < 64; ++index) {
        for (int index2 = 0; index2 < 64; ++index2) {
            ss->searchHistory[WHITE][index][index2] = 0;
            ss->searchHistory[BLACK][index][index2] = 0;
        }
    }
    std::memset(ss->cont_hist, 0, sizeof(ss->cont_hist));
}
