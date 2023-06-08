#include "eval.h"

// if we don't have enough material to mate consider the position a draw
bool MaterialDraw(const S_Board* pos) {
	//If we only have kings on the board then it's a draw
	if (CountBits(Occupancy(pos, BOTH)) == 2)
		return true;
	//KN v K, KB v K
	else if (CountBits(Occupancy(pos, BOTH)) == 3 && ((CountBits(GetPieceBB(pos, KNIGHT)) == 1) || (CountBits(GetPieceBB(pos, BISHOP)) == 1)))
		return true;
	//If we have 4 pieces on the board
	else if (CountBits(Occupancy(pos, BOTH)) == 4) {
		//KNN v K, KN v KN
		if ((CountBits(GetPieceBB(pos, KNIGHT)) == 2))
			return true;
		//KB v KB
		else if (((CountBits(GetPieceBB(pos, BISHOP)) == 2)) && CountBits(GetPieceColorBB(pos, BISHOP, WHITE)) == 1)
			return true;
	}

	return false;
}

static inline float MaterialScale(const S_Board* pos)
{
	return 700 + GetMaterialValue(pos) / 32;
}

// position evaluation
int EvalPosition(const S_Board* pos) {
	bool stm = (pos->side == WHITE);
	int eval = nnue.output(pos->accumulator, stm);
	eval = (eval * MaterialScale(pos)) / 1024;
	return eval;
}