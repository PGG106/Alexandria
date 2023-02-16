#include "movepicker.h"

void PickMove(S_MOVELIST* move_list, const int moveNum) {
	S_MOVE temp;
	int index = 0;
	int bestScore = -MAXSCORE;
	int bestNum = moveNum;
	//starting at the number of the current move and stopping at the end of the list
	for (index = moveNum; index < move_list->count; ++index) {
		//if we find a move with a better score than our bestmove we use that as the new best move
		if (move_list->moves[index].score > bestScore) {
			bestScore = move_list->moves[index].score;
			bestNum = index;
		}
	}
	//swap the move with the best score with the move in place moveNum
	temp = move_list->moves[moveNum];
	move_list->moves[moveNum] = move_list->moves[bestNum];
	move_list->moves[bestNum] = temp;
}