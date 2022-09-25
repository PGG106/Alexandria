#include "makemove.h"
#include "Board.h"
#include "hashkey.h"
#include "init.h"
#include "io.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "stdio.h"
#include "string.h"
#include "uci.h"

//Remove a piece from a square
void ClearPiece(const int piece, const int sq, S_Board* pos) {

	int color = Color[piece];
	HASH_PCE(piece, sq);
	pop_bit(pos->bitboards[piece], sq);
	pos->pieces[sq] = EMPTY;
	pop_bit(pos->occupancies[BOTH], sq);
	pop_bit(pos->occupancies[color], sq);
}

//Add a piece to a square
void AddPiece(const int piece, const int to, S_Board* pos) {

	int color = Color[piece];
	// ♦set up promoted piece on chess board
	set_bit(pos->bitboards[piece], to);
	set_bit(pos->occupancies[color], to);
	set_bit(pos->occupancies[BOTH], to);
	pos->pieces[to] = piece;
	HASH_PCE(piece, to);
}

//Remove a piece from a square while also deactivating the nnue weights tied to the piece
void ClearPieceNNUE(const int piece, const int sq, S_Board* pos) {

	int color = Color[piece];
	if (piece != EMPTY && pos->pieces[sq] != EMPTY)
		nnue.deactivate(piece, sq);
	HASH_PCE(piece, sq);
	pop_bit(pos->bitboards[piece], sq);
	pos->pieces[sq] = EMPTY;
	pop_bit(pos->occupancies[BOTH], sq);
	pop_bit(pos->occupancies[color], sq);
}

//Add a piece to a square while also activating the nnue weights tied to the piece
void AddPieceNNUE(const int piece, const int to, S_Board* pos) {

	int color = Color[piece];
	if (piece != EMPTY && pos->pieces[to] == EMPTY) {

		nnue.activate(piece, to);

	}

	// ♦set up promoted piece on chess board
	set_bit(pos->bitboards[piece], to);
	set_bit(pos->occupancies[color], to);
	set_bit(pos->occupancies[BOTH], to);
	pos->pieces[to] = piece;
	HASH_PCE(piece, to);
}

//Move a piece from square to to square from without updating the NNUE weights
void MovePiece(const int piece, const int from, const int to, S_Board* pos) {

	ClearPiece(piece, from, pos);
	AddPiece(piece, to, pos);
}

//Move a piece from square to to square from
void MovePieceNNUE(const int piece, const int from, const int to, S_Board* pos) {
	nnue.move(piece, from, to);
	MovePiece(piece, from, to, pos);
}


// make move on chess board
int make_move(int move, S_Board* pos) {

	pos->history[pos->hisPly].fiftyMove = pos->fiftyMove;
	pos->history[pos->hisPly].enPas = pos->enPas;
	pos->history[pos->hisPly].castlePerm = pos->castleperm;
	pos->history[pos->hisPly].posKey = pos->posKey;
	pos->history[pos->hisPly].move = move;
	accumulatorStack.emplace_back(nnue.whiteAccumulator);
	accumulatorStackBlack.emplace_back(nnue.blackAccumulator);
	// parse move
	int source_square = get_move_source(move);
	int target_square = get_move_target(move);
	int piece = get_move_piece(move);
	int promoted_piece = get_move_promoted(move);

	int capture = get_move_capture(move);
	int double_push = !(abs(target_square - source_square) - 16) && ((piece == WP) || (piece == BP));
	int enpass = isEnpassant(pos, move);
	int castling = (((piece == WK) || (piece == BK)) && (abs(target_square - source_square) == 2));

	pos->fiftyMove++;

	// handling capture moves
	if (capture) {
		int piececap = pos->pieces[target_square];

		ClearPieceNNUE(piececap, target_square, pos);

		pos->history[pos->hisPly].capture = piececap;

		pos->fiftyMove = 0;
	}

	//if a pawn was moves reset the 50 move rule counter
	if (piece == WP || piece == BP)
		pos->fiftyMove = 0;

	// move piece
	MovePieceNNUE(piece, source_square, target_square, pos);

	pos->hisPly++;
	pos->ply++;

	// handle pawn promotions
	if (promoted_piece) {
		if (pos->side == WHITE)
			ClearPieceNNUE(WP, target_square, pos);

		else
			ClearPieceNNUE(BP, target_square, pos);

		// set up promoted piece on chess board
		AddPieceNNUE(promoted_piece, target_square, pos);
	}

	// handle enpassant captures
	if (enpass) {
		if (pos->side == WHITE) {
			ClearPieceNNUE(BP, target_square + 8, pos);
		}
		else {
			ClearPieceNNUE(WP, target_square - 8, pos);
		}
	}
	if (pos->enPas != no_sq)
		HASH_EP;
	// reset enpassant square
	pos->enPas = no_sq;

	// handle double pawn push
	if (double_push) {
		if (pos->side == WHITE) {
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
	if (castling) {
		// switch target square
		switch (target_square) {
			// white castles king side
		case (g1):
			// move H rook
			MovePieceNNUE(WR, h1, f1, pos);
			break;

			// white castles queen side
		case (c1):
			// move A rook
			MovePieceNNUE(WR, a1, d1, pos);
			break;

			// black castles king side
		case (g8):
			// move H rook
			MovePieceNNUE(BR, h8, f8, pos);
			break;

			// black castles queen side
		case (c8):
			// move A rook
			MovePieceNNUE(BR, a8, d8, pos);
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

int Unmake_move(S_Board* pos) {

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

	int enpass = isEnpassant(pos, move);
	int castling = (((piece == WK) || (piece == BK)) && (abs(target_square - source_square) == 2));
	int piececap = pos->history[pos->hisPly].capture;

	nnue.whiteAccumulator = accumulatorStack.back();
	nnue.blackAccumulator = accumulatorStackBlack.back();
	accumulatorStack.pop_back();
	accumulatorStackBlack.pop_back();

	// handle pawn promotions
	if (promoted_piece) {
		ClearPiece(promoted_piece, target_square, pos);
	}

	// move piece
	MovePiece(piece, target_square, source_square, pos);

	// handle enpassant captures
	if (enpass) {

		// erase the pawn depending on side to move
		(pos->side == BLACK) ? AddPiece(BP, target_square + 8, pos)
			: AddPiece(WP, target_square - 8, pos);
	}

	// handle castling moves
	if (castling) {
		// switch target square
		switch (target_square) {
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
	if (capture && !enpass) {
		AddPiece(piececap, target_square, pos);
	}

	// change side
	pos->side ^= 1;
	// restore zobrist key (done at the end to avoid overwriting the value while
	// moving pieces bacl to their place)

	pos->posKey = pos->history[pos->hisPly].posKey;

	return 1;
}

//MakeNullMove handles the playing of a null move (a move that doesn't move any piece)
void MakeNullMove(S_Board* pos) {
	pos->ply++;
	pos->history[pos->hisPly].posKey = pos->posKey;

	if (pos->enPas != no_sq)
		HASH_EP;

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

//Take back a null move
void TakeNullMove(S_Board* pos) {

	pos->hisPly--;
	pos->ply--;

	pos->castleperm = pos->history[pos->hisPly].castlePerm;
	pos->fiftyMove = pos->history[pos->hisPly].fiftyMove;
	pos->enPas = pos->history[pos->hisPly].enPas;

	pos->side ^= 1;
	pos->posKey = pos->history[pos->hisPly].posKey;
}