#include "Board.h"
#include "move.h"
#include "History.h"

//Update the history heuristics of all the quiet moves passed to the function
void updateHH(S_Board* pos, int depth, int bestmove, S_MOVELIST* quiet_moves) {
	int bonus = depth * depth;
	bonus = bonus - getHHScore(pos, bestmove) * abs(bonus) / 32768;
	//Loop through all the quiet moves
	for (int i = 0; i < quiet_moves->count; i++) {
		int move = quiet_moves->moves[i].move;
		if (move == bestmove)
			//if we are at a depth >1 increment the history score of the best move
			pos->searchHistory[pos->pieces[get_move_source(bestmove)]]
			[get_move_target(bestmove)] += bonus;
		else { // if the move isn't the best move decrease its history score
			pos->searchHistory[pos->pieces[get_move_source(move)]]
				[get_move_target(move)] -= bonus;
		}
	}
}

//Returns the history score of a move
int getHHScore(S_Board* pos, int  move) {
	return pos->searchHistory[pos->pieces[get_move_source(move)]][get_move_target(move)];
}