#include "History.h"
void updateHHScore(const S_Board* pos, Search_data* ss, int move, int bonus) {
	int scaled_bonus = bonus - getHHScore(pos, ss, move) * std::abs(bonus) / 32768;
	//Update move score
	ss->searchHistory[pos->pieces[From(move)]]
		[To(move)] += scaled_bonus;
}

//Update the history heuristics of all the quiet moves passed to the function
void updateHH(const S_Board* pos, Search_data* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves) {
	//define the history bonus
	int bonus = std::min(150 * depth, 1500);
	//increase bestmove HH score
	updateHHScore(pos, ss, bestmove, bonus);
	//Loop through all the quiet moves
	for (int i = 0; i < quiet_moves->count; i++) {
		int move = quiet_moves->moves[i].move;
		if (move == bestmove) continue;
		updateHHScore(pos, ss, move, -bonus);
	}
}

//Returns the history score of a move
int getHHScore(const S_Board* pos, const Search_data* sd, const int  move) {
	return sd->searchHistory[pos->pieces[From(move)]][To(move)];
}
//Resets the history table
void cleanHistory(Search_data* ss) {
	//For every piece [12] moved to every square [64] we reset the searchHistory value
	for (int index = 0; index < 12; ++index) {
		for (int index2 = 0; index2 < 64; ++index2) {
			ss->searchHistory[index][index2] = 0;
		}
	}
}