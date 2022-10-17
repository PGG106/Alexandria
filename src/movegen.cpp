#include "movegen.h"
#include "Board.h"
#include "PieceData.h"
#include "attack.h"
#include "magic.h"
#include "makemove.h"
#include "move.h"
#include "stdint.h"

// is the square given in input attacked by the current given side
bool is_square_attacked(const S_Board* pos, int square, int side) {
	//Take the occupancies of obth positions, encoding where all the pieces on the board reside
	Bitboard occ = pos->occupancies[BOTH];
	// is the square attacked by white pawns
	if ((side == WHITE) && (pawn_attacks[BLACK][square] & GetPawnsColorBB(pos, WHITE)))
		return TRUE;
	// is the square attacked by black pawns
	else if ((side == BLACK) &&
		(pawn_attacks[WHITE][square] & GetPawnsColorBB(pos, BLACK)))
		return TRUE;
	// is the square attacked by queens
	if (get_queen_attacks(square, occ) & GetQueensColorBB(pos, side))
		return TRUE;
	// is the square attacked by rooks
	if (get_rook_attacks(square, occ) & GetRooksColorBB(pos, side))
		return TRUE;
	// is the square attacked by bishops
	if (get_bishop_attacks(square, occ) & GetBishopsColorBB(pos, side))
		return TRUE;
	// is the square attacked by knights
	if (knight_attacks[square] & GetKnightsColorBB(pos, side))
		return TRUE;
	// is the square attacked by kings
	if (king_attacks[square] & GetKingColorBB(pos, side))
		return TRUE;
	// by default return false
	return FALSE;
}

static inline Bitboard PawnPush(int color, int sq) {
	if (color == WHITE)
		return (1ULL << (sq - 8));
	return (1ULL << (sq + 8));
}

static inline void init(S_Board* pos, int color, int sq) {
	Bitboard newMask = DoCheckmask(pos, color, sq);
	pos->checkMask = newMask ? newMask : 18446744073709551615ULL;
	DoPinMask(pos, color, sq);
}
//Check for move legality by generating the list of legal moves in a position and checking if that move is present
int MoveExists(S_Board* pos, const int move) {
	S_MOVELIST list[1];
	generate_moves(list, pos);

	int MoveNum = 0;
	for (MoveNum = 0; MoveNum < list->count; ++MoveNum) {
		if (list->moves[MoveNum].move == move) {
			return TRUE;
		}
	}
	return FALSE;
}
// function that adds a move to the move list
static inline void AddMove(const S_Board* pos, int move,
	S_MOVELIST* list) {
	list->moves[list->count].move = move;
	list->moves[list->count].score = 0;
	list->count++;
}
// function that adds a pawn move (and all its possible branches) to the move list
static inline void AddPawnMove(const S_Board* pos, const int from, const int to, S_MOVELIST* list) {
	int capture = (pos->pieces[to] != EMPTY);


	if (pos->side == WHITE) {
		if (from >= a7 &&
			from <= h7) { // if the piece is moving from the 7th to the 8th rank
			AddMove(pos, encode_move(from, to, WP, WQ, capture), list);
			AddMove(pos, encode_move(from, to, WP, WR, capture), list); // consider every possible piece promotion
			AddMove(pos, encode_move(from, to, WP, WB, capture), list);
			AddMove(pos, encode_move(from, to, WP, WN, capture), list);
		}
		else { // else do not include possible promotions
			AddMove(pos, encode_move(from, to, WP, 0, capture), list);
		}
	}

	else {
		if (from >= a2 &&
			from <= h2) { // if the piece is moving from the 2nd to the 1st rank
			AddMove(pos, encode_move(from, to, BP, BQ, capture), list);
			AddMove(pos, encode_move(from, to, BP, BR, capture), list); // consider every possible piece promotion
			AddMove(pos, encode_move(from, to, BP, BB, capture), list);
			AddMove(pos, encode_move(from, to, BP, BN, capture), list);
		}
		else { // else do not include possible promotions
			AddMove(pos, encode_move(from, to, BP, 0, capture), list);
		}
	}
}

static inline Bitboard LegalPawnMoves(S_Board* pos, int color, int square) {
	Bitboard enemy = pos->occupancies[color ^ 1];

	// If we are pinned diagonally we can only do captures which are on the pin_dg
	// and on the checkmask
	if (pos->pinD & (1ULL << square))
		return pawn_attacks[color][square] & pos->pinD & pos->checkMask & enemy;
	// Calculate pawn pushs
	Bitboard push = PawnPush(color, square) & ~pos->occupancies[2];

	push |=
		(color == WHITE)
		? (get_rank[square] == 1 ? (push >> 8) & ~pos->occupancies[2] : 0ULL)
		: (get_rank[square] == 6 ? (push << 8) & ~pos->occupancies[2] : 0ULL);

	// If we are pinned horizontally we can do no moves but if we are pinned
	// vertically we can only do pawn pushs
	if (pos->pinHV & (1ULL << square))
		return push & pos->pinHV & pos->checkMask;
	int8_t offset = color * -16 + 8;
	Bitboard attacks = pawn_attacks[color][square];
	// If we are in check and  the en passant square lies on our attackmask and
	// the en passant piece gives check return the ep mask as a move square
	if (pos->checkMask != 18446744073709551615ULL && pos->enPas != no_sq &&
		attacks & (1ULL << pos->enPas) &&
		pos->checkMask & (1ULL << (pos->enPas + offset)))
		return (attacks & (1ULL << pos->enPas));
	// If we are in check we can do all moves that are on the checkmask
	if (pos->checkMask != 18446744073709551615ULL)
		return ((attacks & enemy) | push) & pos->checkMask;

	Bitboard moves = ((attacks & enemy) | push) & pos->checkMask;
	// We need to make extra sure that ep moves dont leave the king in check
	// 7k/8/8/K1Pp3r/8/8/8/8 w - d6 0 1
	// Horizontal rook pins our pawn through another pawn, our pawn can push but
	// not take enpassant remove both the pawn that made the push and our pawn
	// that could take in theory and check if that would give check
	if (pos->enPas != no_sq && square_distance(square, pos->enPas) == 1 &&
		(1ULL << pos->enPas) & attacks) {
		int ourPawn = GetPiece(PAWN, color);
		int theirPawn = GetPiece(PAWN, color ^ 1);
		int kSQ = KingSQ(pos, color);
		ClearPiece(ourPawn, square, pos);
		ClearPiece(theirPawn, (pos->enPas + offset), pos);
		AddPiece(ourPawn, pos->enPas, pos);
		if (!((get_rook_attacks(kSQ, pos->occupancies[2]) &
			(GetRooksColorBB(pos, color ^ 1) |
				GetQueensColorBB(pos, color ^ 1)))))
			moves |= (1ULL << pos->enPas);
		AddPiece(ourPawn, square, pos);
		AddPiece(theirPawn, pos->enPas + offset, pos);
		ClearPiece(ourPawn, pos->enPas, pos);
	}
	return moves;
}

static inline Bitboard LegalKnightMoves(S_Board* pos, int color, int square) {
	if (pos->pinD & (1ULL << square) || pos->pinHV & (1ULL << square))
		return NOMOVE;
	return knight_attacks[square] & ~(pos->occupancies[pos->side]) &
		pos->checkMask;
}

static inline Bitboard LegalBishopMoves(S_Board* pos, int color, int square) {
	if (pos->pinHV & (1ULL << square))
		return NOMOVE;
	if (pos->pinD & (1ULL << square))
		return get_bishop_attacks(square, pos->occupancies[BOTH]) &
		~(pos->occupancies[pos->side]) & pos->pinD & pos->checkMask;
	return get_bishop_attacks(square, pos->occupancies[BOTH]) &
		~(pos->occupancies[pos->side]) & pos->checkMask;
}

static inline Bitboard LegalRookMoves(S_Board* pos, int color, int square) {
	if (pos->pinD & (1ULL << square))
		return NOMOVE;
	if (pos->pinHV & (1ULL << square))
		return get_rook_attacks(square, pos->occupancies[BOTH]) &
		~(pos->occupancies[pos->side]) & pos->pinHV & pos->checkMask;
	return get_rook_attacks(square, pos->occupancies[BOTH]) &
		~(pos->occupancies[pos->side]) & pos->checkMask;
}

static inline Bitboard LegalQueenMoves(S_Board* pos, int color, int square) {
	return LegalRookMoves(pos, color, square) |
		LegalBishopMoves(pos, color, square);
}

static inline Bitboard LegalKingMoves(S_Board* pos, int color, int square) {
	Bitboard moves = king_attacks[square] & ~pos->occupancies[color];
	Bitboard final_moves = NOMOVE;
	int king = GetPiece(KING, color);
	ClearPiece(king, square, pos);
	while (moves) {
		int index = get_ls1b_index(moves);
		pop_bit(moves, index);
		if (is_square_attacked(pos, index, pos->side ^ 1)) {
			continue;
		}
		final_moves |= (1ULL << index);
	}
	AddPiece(king, square, pos);

	return final_moves;
}

// generate all moves
void generate_moves(S_MOVELIST* move_list, S_Board* pos) { // init move count
	move_list->count = 0;

	// define source & target squares
	int source_square, target_square;

	init(pos, pos->side, KingSQ(pos, pos->side));

	if (pos->checks < 2) {
		Bitboard pawns = GetPawnsColorBB(pos, pos->side);

		while (pawns) {
			// init source square
			source_square = get_ls1b_index(pawns);

			Bitboard moves = LegalPawnMoves(pos, pos->side, source_square);
			while (moves) {
				// init target square
				target_square = get_ls1b_index(moves);
				AddPawnMove(pos, source_square, target_square, move_list);
				pop_bit(moves, target_square);
			}

			// pop ls1b from piece bitboard copy
			pop_bit(pawns, source_square);
		}

		// genarate knight moves
		Bitboard knights = GetKnightsColorBB(pos, pos->side);

		while (knights) {
			source_square = get_ls1b_index(knights);
			Bitboard moves = LegalKnightMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(KNIGHT, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(knights, source_square);
		}

		Bitboard bishops = GetBishopsColorBB(pos, pos->side);

		while (bishops) {
			source_square = get_ls1b_index(bishops);
			Bitboard moves = LegalBishopMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(BISHOP, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(bishops, source_square);
		}

		Bitboard rooks = GetRooksColorBB(pos, pos->side);

		while (rooks) {
			source_square = get_ls1b_index(rooks);
			Bitboard moves = LegalRookMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(ROOK, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(rooks, source_square);
		}

		Bitboard queens = GetQueensColorBB(pos, pos->side);
		while (queens) {
			source_square = get_ls1b_index(queens);
			Bitboard moves = LegalQueenMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(QUEEN, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(queens, source_square);
		}
	}
	source_square = KingSQ(pos, pos->side);
	int piece = GetPiece(KING, pos->side);

	Bitboard moves = LegalKingMoves(pos, pos->side, source_square);
	while (moves) {
		int target_square = get_ls1b_index(moves);
		int capture = (pos->pieces[target_square] != EMPTY);

		pop_bit(moves, target_square);
		AddMove(
			pos,
			encode_move(source_square, target_square, piece, 0, capture),
			move_list);
	}

	if (pos->checkMask == 18446744073709551615ULL) {
		if (pos->side == WHITE) {
			// king side castling is available
			if (pos->castleperm & WKCA) {
				// make sure square between king and king's rook are empty
				if (!get_bit(pos->occupancies[BOTH], f1) &&
					!get_bit(pos->occupancies[BOTH], g1)) {
					// make sure king and the f1 squares are not under attacks
					if (!is_square_attacked(pos, e1, BLACK) &&
						!is_square_attacked(pos, f1, BLACK) &&
						!is_square_attacked(pos, g1, BLACK))
						AddMove(pos, encode_move(e1, g1, WK, 0, 0), move_list);
				}
			}

			if (pos->castleperm & WQCA) {
				// make sure square between king and queen's rook are empty
				if (!get_bit(pos->occupancies[BOTH], d1) &&
					!get_bit(pos->occupancies[BOTH], c1) &&
					!get_bit(pos->occupancies[BOTH], b1)) {
					// make sure king and the d1 squares are not under attacks
					if (!is_square_attacked(pos, e1, BLACK) &&
						!is_square_attacked(pos, d1, BLACK) &&
						!is_square_attacked(pos, c1, BLACK))
						AddMove(pos, encode_move(e1, c1, WK, 0, 0), move_list);
				}
			}
		}

		else {
			if (pos->castleperm & BKCA) {
				// make sure square between king and king's rook are empty
				if (!get_bit(pos->occupancies[BOTH], f8) &&
					!get_bit(pos->occupancies[BOTH], g8)) {
					// make sure king and the f8 squares are not under attacks
					if (!is_square_attacked(pos, e8, WHITE) &&
						!is_square_attacked(pos, f8, WHITE) &&
						!is_square_attacked(pos, g8, WHITE))
						AddMove(pos, encode_move(e8, g8, BK, 0, 0), move_list);
				}
			}

			if (pos->castleperm & BQCA) {
				// make sure square between king and queen's rook are empty
				if (!get_bit(pos->occupancies[BOTH], d8) &&
					!get_bit(pos->occupancies[BOTH], c8) &&
					!get_bit(pos->occupancies[BOTH], b8)) {
					// make sure king and the d8 squares are not under attacks
					if (!is_square_attacked(pos, e8, WHITE) &&
						!is_square_attacked(pos, d8, WHITE) &&
						!is_square_attacked(pos, c8, WHITE))
						AddMove(pos, encode_move(e8, c8, BK, 0, 0), move_list);
				}
			}
		}
	}
}

// generate all moves
void generate_captures(S_MOVELIST* move_list, S_Board* pos) {
	// init move count
	move_list->count = 0;

	// define source & target squares
	int source_square, target_square;

	init(pos, pos->side, KingSQ(pos, pos->side));

	if (pos->checks < 2) {
		Bitboard pawn_mask = GetPawnsColorBB(pos, pos->side);
		Bitboard knights_mask = GetKnightsColorBB(pos, pos->side);
		Bitboard bishops_mask = GetBishopsColorBB(pos, pos->side);
		Bitboard rooks_mask = GetRooksColorBB(pos, pos->side);
		Bitboard queens_mask = GetQueensColorBB(pos, pos->side);

		while (pawn_mask) {
			// init source square
			source_square = get_ls1b_index(pawn_mask);

			Bitboard moves =
				LegalPawnMoves(pos, pos->side, source_square) &
				(pos->occupancies[pos->side ^ 1] | 255 | 18374686479671623680ULL);
			while (moves) {
				// init target square
				target_square = get_ls1b_index(moves);
				AddPawnMove(pos, source_square, target_square, move_list);
				pop_bit(moves, target_square);
			}

			// pop ls1b from piece bitboard copy
			pop_bit(pawn_mask, source_square);
		}
		// genarate knight moves
		while (knights_mask) {
			source_square = get_ls1b_index(knights_mask);
			Bitboard moves = LegalKnightMoves(pos, pos->side, source_square) &
				(pos->occupancies[pos->side ^ 1]);
			//while we have moves that the knight can play we add them to the list
			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(KNIGHT, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(knights_mask, source_square);
		}

		while (bishops_mask) {
			source_square = get_ls1b_index(bishops_mask);
			Bitboard moves = LegalBishopMoves(pos, pos->side, source_square) &
				(pos->occupancies[pos->side ^ 1]);

			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(BISHOP, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(bishops_mask, source_square);
		}

		while (rooks_mask) {
			source_square = get_ls1b_index(rooks_mask);
			Bitboard moves = LegalRookMoves(pos, pos->side, source_square) &
				(pos->occupancies[pos->side ^ 1]);

			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(ROOK, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(rooks_mask, source_square);
		}

		while (queens_mask) {
			source_square = get_ls1b_index(queens_mask);
			Bitboard moves = LegalQueenMoves(pos, pos->side, source_square) &
				(pos->occupancies[pos->side ^ 1]);

			while (moves) {
				target_square = get_ls1b_index(moves);
				int capture = (pos->pieces[target_square] != EMPTY);
				int piece = GetPiece(QUEEN, pos->side);
				AddMove(pos,
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(queens_mask, source_square);
		}
	}
	source_square = KingSQ(pos, pos->side);
	int piece = GetPiece(KING, pos->side);

	Bitboard moves = LegalKingMoves(pos, pos->side, source_square) &
		(pos->occupancies[pos->side ^ 1]);
	while (moves) {
		int target_square = get_ls1b_index(moves);
		int capture = (pos->pieces[target_square] != EMPTY);

		pop_bit(moves, target_square);
		AddMove(
			pos,
			encode_move(source_square, target_square, piece, 0, capture),
			move_list);
	}
}
