#include "Board.h"
#include "move.h"
#include "History.h"

//Update the history heuristics of all the quiet moves passed to the function
void updateHH(S_Board* pos, S_Stack* ss, int depth, int bestmove, S_MOVELIST* quiet_moves) {
	int bonus = depth * depth;
	//Loop through all the quiet moves
	for (int i = 0; i < quiet_moves->count; i++) {
		int move = quiet_moves->moves[i].move;
		bonus = bonus - getHHScore(pos, ss, move) * abs(bonus) / 32768;
		if (move == bestmove)
			//if we are at a depth >1 increment the history score of the best move
			ss->searchHistory[pos->pieces[From(bestmove)]]
			[To(bestmove)] += bonus;
		else { // if the move isn't the best move decrease its history score
			ss->searchHistory[pos->pieces[From(move)]]
				[To(move)] -= bonus;
		}
	}
}

//Returns the history score of a move
int getHHScore(S_Board* pos, S_Stack* ss, int  move) {
	return ss->searchHistory[pos->pieces[From(move)]][To(move)];
}

void cleanHistory(S_Stack* ss) {
	//For every piece [12] moved to every square [64] we reset the searchHistory value
	for (int index = 0; index < 12; ++index) {
		for (int index2 = 0; index2 < 64; ++index2) {
			ss->searchHistory[index][index2] = 0;
		}
	}
}