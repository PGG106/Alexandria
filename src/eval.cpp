#include "eval.h"

// if we don't have enough material to mate consider the position a draw
int MaterialDraw(const S_Board* pos) {
	//If we only have kings on the board then it's a draw
	if (CountBits(Occupancy(pos, BOTH)) == 2)
		return TRUE;
	else if (CountBits(Occupancy(pos, BOTH)) == 3 && ((CountBits(GetPieceBB(pos, KNIGHT)) == 1) || (CountBits(GetPieceBB(pos, BISHOP)) == 1)))
		return TRUE;

	return FALSE;
}

// position evaluation
int EvalPosition(const S_Board* pos) {
	return (pos->side == WHITE) ? nnue.output(pos->accumulator) : -nnue.output(pos->accumulator);
}