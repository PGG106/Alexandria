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
	else if (count_bits(pos->occupancies[BOTH]) == 3 && ((count_bits(GetKnightsBB(pos)) == 1) || (count_bits(GetBishopsBB(pos)) == 1)))
		return TRUE;

	/*
	int white_rooks = count_bits(pos->bitboards[WR]);
	int black_rooks = count_bits(pos->bitboards[BR]);
	int white_bishops = count_bits(pos->bitboards[WB]);
	int black_bishops = count_bits(pos->bitboards[BB]);
	int white_knights = count_bits(pos->bitboards[WN]);
	int black_knights = count_bits(pos->bitboards[BN]);
	int white_queens = count_bits(pos->bitboards[WQ]);
	int black_queens = count_bits(pos->bitboards[BQ]);
	*/
	return FALSE;
}

// position evaluation
int EvalPosition(const S_Board* pos) {
	return (pos->side == WHITE) ? nnue.output() : -nnue.output();
}