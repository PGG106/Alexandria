#include "move.h"
#include "Board.h"
#include "makemove.h"
#include "movegen.h"
#include "init.h"
#include "string.h"
#include "stdio.h"
#include "io.h"
#include "init.h"
#include "search.h"
#include "uci.h"
#include "hashkey.h"

void ClearPiece(const int piece, const int sq, S_Board* pos) {

	int color = Color[piece];
	if (piece != EMPTY && pos->pieces[sq] != EMPTY)
		nnue.deactivate(sq + piece * 64);
	HASH_PCE(piece, sq);
	pop_bit(pos->bitboards[piece], sq);
	pos->pieces[sq] = EMPTY;
	pop_bit(pos->occupancies[BOTH], sq);
	pop_bit(pos->occupancies[color], sq);

}


void AddPiece(const int piece, const int to, S_Board* pos) {


	int color = Color[piece];
	if (piece != EMPTY && pos->pieces[to] == EMPTY)
		nnue.activate(to + piece * 64);
	// ♦set up promoted piece on chess board
	set_bit(pos->bitboards[piece], to);
	set_bit(pos->occupancies[color], to);
	set_bit(pos->occupancies[BOTH], to);
	pos->pieces[to] = piece;
	HASH_PCE(piece, to);


}



void MovePiece(const int piece, const int from, const int to, S_Board* pos) {


	ClearPiece(piece, from, pos);
	AddPiece(piece, to, pos);

}


// make move on chess board
int make_move(int move, S_Board* pos)
{


	pos->history[pos->hisPly].fiftyMove = pos->fiftyMove;
	pos->history[pos->hisPly].enPas = pos->enPas;
	pos->history[pos->hisPly].castlePerm = pos->castleperm;
	pos->history[pos->hisPly].posKey = pos->posKey;
	pos->history[pos->hisPly].move = move;

	// parse move
	int source_square = get_move_source(move);
	int target_square = get_move_target(move);
	int piece = get_move_piece(move);
	int promoted_piece = get_move_promoted(move);

	int capture = get_move_capture(move);


	int double_push = get_move_double(move);
	int enpass = get_move_enpassant(move);
	int castling = get_move_castling(move);

	pos->fiftyMove++;

	// handling capture moves
	if (capture)
	{
		int piececap = pos->pieces[target_square];

		ClearPiece(piececap, target_square, pos);

		pos->history[pos->hisPly].capture = piececap;

		pos->fiftyMove = 0;
	}

	if (piece == WP || piece == BP)
		pos->fiftyMove = 0;

	// move piece
	MovePiece(piece, source_square, target_square, pos);

	pos->hisPly++;
	pos->ply++;

	// handle pawn promotions
	if (promoted_piece)
	{
		if (pos->side == WHITE) ClearPiece(WP, target_square, pos);

		else ClearPiece(BP, target_square, pos);

		// set up promoted piece on chess board
		AddPiece(promoted_piece, target_square, pos);

	}

	// handle enpassant captures
	if (enpass)
	{
		if (pos->side == WHITE) {
			ClearPiece(BP, target_square + 8, pos);
		}
		else {
			ClearPiece(WP, target_square - 8, pos);
		}
	}
	if (pos->enPas != no_sq) HASH_EP;
	// reset enpassant square
	pos->enPas = no_sq;


	// handle double pawn push
	if (double_push)
	{
		if (pos->side == WHITE)
		{
			// set enpassant square
			pos->enPas = target_square + 8;

			// hash enpassant
			HASH_EP;

		}
		else {

			// set enpassant square
			pos->enPas = target_square - 8;

			// hash enpassant
			HASH_EP;
		}
	}


	// handle castling moves
	if (castling)
	{
		// switch target square
		switch (target_square)
		{
			// white castles king side
		case (g1):
			// move H rook
			MovePiece(WR, h1, f1, pos);
			break;

			// white castles queen side
		case (c1):
			// move A rook
			MovePiece(WR, a1, d1, pos);
			break;

			// black castles king side
		case (g8):
			// move H rook
			MovePiece(BR, h8, f8, pos);
			break;

			// black castles queen side
		case (c8):
			// move A rook
			MovePiece(BR, a8, d8, pos);
			break;
		}
	}
	HASH_CA;
	// update castling rights
	pos->castleperm &= castling_rights[source_square];
	pos->castleperm &= castling_rights[target_square];

	HASH_CA;


	// change side
	pos->side ^= 1;
	HASH_SIDE;
	//



	return 1;

}





int Unmake_move(S_Board* pos)
{

	// quiet moves

	pos->hisPly--;
	pos->ply--;

	pos->enPas = pos->history[pos->hisPly].enPas;
	pos->fiftyMove = pos->history[pos->hisPly].fiftyMove;
	pos->castleperm = pos->history[pos->hisPly].castlePerm;

	int move = pos->history[pos->hisPly].move;


	// parse move
	int source_square = get_move_source(move);
	int target_square = get_move_target(move);
	int piece = get_move_piece(move);
	int promoted_piece = get_move_promoted(move);
	int capture = get_move_capture(move);

	int enpass = get_move_enpassant(move);
	int castling = get_move_castling(move);
	int piececap = pos->history[pos->hisPly].capture;



	// handle pawn promotions
	if (promoted_piece)
	{
		ClearPiece(promoted_piece, target_square, pos);

	}



	// move piece
	MovePiece(piece, target_square, source_square, pos);


	// handle enpassant captures
	if (enpass)
	{

		// erase the pawn depending on side to move
		(pos->side == BLACK) ? AddPiece(BP, target_square + 8, pos) :
			AddPiece(WP, target_square - 8, pos);

	}



	// handle castling moves
	if (castling)
	{
		// switch target square
		switch (target_square)
		{
			// white castles king side
		case (g1):
			// move H rook
			MovePiece(WR, f1, h1, pos);
			break;

			// white castles queen side
		case (c1):
			// move A rook
			MovePiece(WR, d1, a1, pos);
			break;

			// black castles king side
		case (g8):
			// move H rook
			MovePiece(BR, f8, h8, pos);
			break;

			// black castles queen side
		case (c8):
			// move A rook
			MovePiece(BR, d8, a8, pos);
			break;
		}


	}


	// handling capture moves
	if (capture && !enpass)
	{
		AddPiece(piececap, target_square, pos);

	}


	// change side
	pos->side ^= 1;
	//restore zobrist key (done at the end to avoid overwriting the value while moving pieces bacl to their place)

	pos->posKey = pos->history[pos->hisPly].posKey;

	return 1;
}



void MakeNullMove(S_Board* pos) {
	pos->ply++;
	pos->history[pos->hisPly].posKey = pos->posKey;

	if (pos->enPas != no_sq) HASH_EP;

	pos->history[pos->hisPly].move = NOMOVE;
	pos->history[pos->hisPly].fiftyMove = pos->fiftyMove;
	pos->history[pos->hisPly].enPas = pos->enPas;
	pos->history[pos->hisPly].castlePerm = pos->castleperm;
	pos->enPas = no_sq;

	pos->side ^= 1;
	pos->hisPly++;
	HASH_SIDE;

	return;
}

void TakeNullMove(S_Board* pos) {

	pos->hisPly--;
	pos->ply--;

	pos->castleperm = pos->history[pos->hisPly].castlePerm;
	pos->fiftyMove = pos->history[pos->hisPly].fiftyMove;
	pos->enPas = pos->history[pos->hisPly].enPas;

	pos->side ^= 1;
	pos->posKey = pos->history[pos->hisPly].posKey;
}