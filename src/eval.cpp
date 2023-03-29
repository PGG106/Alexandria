#include "eval.h"

// if we don't have enough material to mate consider the position a draw
bool MaterialDraw(const S_Board* pos) {
	//If we only have kings on the board then it's a draw
	if (CountBits(Occupancy(pos, BOTH)) == 2)
		return true;
	//KN v K, KB v K
	else if (CountBits(Occupancy(pos, BOTH)) == 3 && ((CountBits(GetPieceBB(pos, KNIGHT)) == 1) || (CountBits(GetPieceBB(pos, BISHOP)) == 1)))
		return true;
	//KNN v K, KN v KN
	else if (CountBits(Occupancy(pos, BOTH)) == 4 && ((CountBits(GetPieceBB(pos, KNIGHT)) == 2)))
		return true;
	return false;
}

// position evaluation
int EvalPosition(const S_Board* pos) {
	return (pos->side == WHITE) ? nnue.output(pos->accumulator) : -nnue.output(pos->accumulator);
}