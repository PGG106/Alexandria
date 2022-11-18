#include "eval.h"
#include "Board.h"
#include "PieceData.h"
#include "attack.h"
#include "io.h"
#include "magic.h"
#include "stdio.h"
#include "stdlib.h"

// if we don't have enough material to mate consider the position a draw
int MaterialDraw(const S_Board* pos) {
	//If we only have kings on the board then it's a draw
	if (count_bits(pos->occupancies[BOTH]) == 2)
		return TRUE;
	else if (count_bits(pos->occupancies[BOTH]) == 3 && ((count_bits(GetPieceBB(pos, KNIGHT)) == 1) || (count_bits(GetPieceBB(pos, BISHOP)) == 1)))
		return TRUE;

	return FALSE;
}

// position evaluation
int EvalPosition(const S_Board* pos) {
	return (pos->side == WHITE) ? nnue.output() : -nnue.output();
}