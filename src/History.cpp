#include "Board.h"
#include "move.h"
#include "History.h"

//Update the history heuristics of all the quiet moves passed to the function
void updateHH(const S_Board* pos, S_Stack* ss, const int depth, const int bestmove, const S_MOVELIST* quiet_moves) {
	//define the history bonus
	int bonus = depth * depth;
	//Loop through all the quiet moves
	for (int i = 0; i < quiet_moves->count; i++) {
		int move = quiet_moves->moves[i].move;
		//Scale the history bonus in order to cap the history value to +-32768
		bonus = bonus - getHHScore(pos, ss, move) * abs(bonus) / 32768;
		//We increase the score for the bestmove
		if (move == bestmove) {
			ss->searchHistory[pos->pieces[From(bestmove)]]
				[To(bestmove)] += bonus;
		}
		// and decrease it for all the others
		else { 
			ss->searchHistory[pos->pieces[From(move)]]
				[To(move)] -= bonus;
		}
	}
}

//Returns the history score of a move
int getHHScore(const S_Board* pos, const S_Stack* ss, const int  move) {
	return ss->searchHistory[pos->pieces[From(move)]][To(move)];
}
//Resets the history table
void cleanHistory(S_Stack* ss) {
	//For every piece [12] moved to every square [64] we reset the searchHistory value
	for (int index = 0; index < 12; ++index) {
		for (int index2 = 0; index2 < 64; ++index2) {
			ss->searchHistory[index][index2] = 0;
		}
	}
}