#include "History.h"
#include <cstring>
void updateHHScore(const S_Board* pos, Search_data* ss, int move, int bonus) {
	int scaled_bonus = bonus - getHHScore(pos, ss, move) * std::abs(bonus) / 32768;
	//Update move score
	ss->searchHistory[pos->pieces[From(move)]]
		[To(move)] += scaled_bonus;
}
void updateCHScore(Search_data* sd, const int previous_move, const int bestmove, const int bonus) {
	//Update move score
	sd->cont_hist[get_move_piece(previous_move)][To(previous_move)]
		[get_move_piece(bestmove)][To(bestmove)] += bonus;
}
//Update the history heuristics of all the quiet moves passed to the function
void updateHH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves) {
	//define the history bonus
	int bonus = std::min(16 * depth * depth, 1200);
	//increase bestmove HH score
	updateHHScore(pos, ss, bestmove, bonus);
	//Loop through all the quiet moves
	for (int i = 0; i < quiet_moves->count; i++) {
		int move = quiet_moves->moves[i].move;
		if (move == bestmove) continue;
		updateHHScore(pos, ss, move, -bonus);
	}
}

//Update the history heuristics of all the quiet moves passed to the function
void updateCH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const int previous_move, const int previous_previous_move) {
	int bonus = std::min(16 * depth * depth, 1200);
	// Score countermove
	if (pos->ply > 0) {
		updateCHScore(ss, previous_move, bestmove, bonus);
		//Score followup
		if (pos->ply > 1)
		{
			updateCHScore(ss, previous_previous_move, bestmove, bonus);
		}
	}
}

//Returns the history score of a move
int getHHScore(const S_Board* pos, const Search_data* sd, const int  move) {
	return sd->searchHistory[pos->pieces[From(move)]][To(move)];
}

//Returns the history score of a move
int64_t getCHScore(const S_Board* pos, const Search_data* sd, const int  move, const int previous_move, const int previous_previous_move) {
	return pos->ply > 0 ? sd->cont_hist[get_move_piece(previous_move)][To(previous_move)]
		[get_move_piece(move)][To(move)] : 0 +
		pos->ply > 1 ? sd->cont_hist[get_move_piece(previous_previous_move)][To(previous_previous_move)]
		[get_move_piece(move)][To(move)] : 0;
}
//Resets the history table
void cleanHistories(Search_data* ss) {
	//For every piece [12] moved to every square [64] we reset the searchHistory value
	for (int index = 0; index < 12; ++index) {
		for (int index2 = 0; index2 < 64; ++index2) {
			ss->searchHistory[index][index2] = 0;
		}
	}
	std::memset(ss->cont_hist, 0, sizeof(ss->cont_hist));
}