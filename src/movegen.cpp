#include "movegen.h"
#include "attack.h"
#include "magic.h"
#include "makemove.h"

// is the square given in input attacked by the current given side
bool IsSquareAttacked(const S_Board* pos, const int square, const int side) {
	//Take the occupancies of obth positions, encoding where all the pieces on the board reside
	Bitboard occ = Occupancy(pos, BOTH);
	// is the square attacked by white pawns
	if ((side == WHITE) && (pawn_attacks[BLACK][square] & GetPieceColorBB(pos, PAWN, WHITE)))
		return TRUE;
	// is the square attacked by black pawns
	else if ((side == BLACK) &&
		(pawn_attacks[WHITE][square] & GetPieceColorBB(pos, PAWN, BLACK)))
		return TRUE;
	// is the square attacked by queens
	if (GetQueenAttacks(square, occ) & GetPieceColorBB(pos, QUEEN, side))
		return TRUE;
	// is the square attacked by rooks
	if (GetRookAttacks(square, occ) & GetPieceColorBB(pos, ROOK, side))
		return TRUE;
	// is the square attacked by bishops
	if (GetBishopAttacks(square, occ) & GetPieceColorBB(pos, BISHOP, side))
		return TRUE;
	// is the square attacked by knights
	if (knight_attacks[square] & GetPieceColorBB(pos, KNIGHT, side))
		return TRUE;
	// is the square attacked by kings
	if (king_attacks[square] & GetPieceColorBB(pos, KING, side))
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
	GenerateMoves(list, pos);

	int MoveNum = 0;
	for (MoveNum = 0; MoveNum < list->count; ++MoveNum) {
		if (list->moves[MoveNum].move == move) {
			return TRUE;
		}
	}
	return FALSE;
}
// function that adds a move to the move list
static inline void AddMove(int move, S_MOVELIST* list) {
	list->moves[list->count].move = move;
	list->moves[list->count].score = 0;
	list->count++;
}
// function that adds a pawn move (and all its possible branches) to the move list
static inline void AddPawnMove(const S_Board* pos, const int from, const int to, S_MOVELIST* list) {
	int capture = PieceOn(pos, to) != EMPTY;

	if (pos->side == WHITE) {
		if (from >= a7 &&
			from <= h7) { // if the piece is moving from the 7th to the 8th rank
			AddMove(encode_move(from, to, WP, WQ, capture), list);
			AddMove(encode_move(from, to, WP, WR, capture), list); // consider every possible piece promotion
			AddMove(encode_move(from, to, WP, WB, capture), list);
			AddMove(encode_move(from, to, WP, WN, capture), list);
		}
		else { // else do not include possible promotions
			AddMove(encode_move(from, to, WP, 0, capture), list);
		}
	}

	else {
		if (from >= a2 &&
			from <= h2) { // if the piece is moving from the 2nd to the 1st rank
			AddMove(encode_move(from, to, BP, BQ, capture), list);
			AddMove(encode_move(from, to, BP, BR, capture), list); // consider every possible piece promotion
			AddMove(encode_move(from, to, BP, BB, capture), list);
			AddMove(encode_move(from, to, BP, BN, capture), list);
		}
		else { // else do not include possible promotions
			AddMove(encode_move(from, to, BP, 0, capture), list);
		}
	}
}

static inline Bitboard LegalPawnMoves(S_Board* pos, int color, int square) {
	Bitboard enemy = pos->occupancies[color ^ 1];

	// If we are pinned diagonally we can only do captures which are on the pin_dg
	// and on the checkmask

	if (pos->pinD & (1ULL << square))
		return pawn_attacks[color][square] & pos->pinD & pos->checkMask & (enemy | (1ULL << GetEpSquare(pos)));
	// Calculate pawn pushs
	Bitboard push = PawnPush(color, square) & ~pos->occupancies[2];

	push |=
		(color == WHITE)
		? (get_rank[square] == 1 ? (push >> 8) & ~Occupancy(pos, BOTH) : 0ULL)
		: (get_rank[square] == 6 ? (push << 8) & ~Occupancy(pos, BOTH) : 0ULL);

	// If we are pinned horizontally we can do no moves but if we are pinned
	// vertically we can only do pawn pushs
	if (pos->pinHV & (1ULL << square))
		return push & pos->pinHV & pos->checkMask;
	int offset = color * -16 + 8;
	Bitboard attacks = pawn_attacks[color][square];
	// If we are in check and  the en passant square lies on our attackmask and
	// the en passant piece gives check return the ep mask as a move square
	if (pos->checkMask != 18446744073709551615ULL && GetEpSquare(pos) != no_sq &&
		attacks & (1ULL << GetEpSquare(pos)) &&
		pos->checkMask & (1ULL << (GetEpSquare(pos) + offset)))
		return (attacks & (1ULL << GetEpSquare(pos)));
	// If we are in check we can do all moves that are on the checkmask
	if (pos->checkMask != 18446744073709551615ULL)
		return ((attacks & enemy) | push) & pos->checkMask;

	Bitboard moves = ((attacks & enemy) | push) & pos->checkMask;
	// We need to make extra sure that ep moves dont leave the king in check
	// 7k/8/8/K1Pp3r/8/8/8/8 w - d6 0 1
	// Horizontal rook pins our pawn through another pawn, our pawn can push but
	// not take enpassant remove both the pawn that made the push and our pawn
	// that could take in theory and check if that would give check
	if (GetEpSquare(pos) != no_sq && SquareDistance(square, GetEpSquare(pos)) == 1 &&
		(1ULL << GetEpSquare(pos)) & attacks) {
		int ourPawn = GetPiece(PAWN, color);
		int theirPawn = GetPiece(PAWN, color ^ 1);
		int kSQ = KingSQ(pos, color);
		ClearPiece(ourPawn, square, pos);
		ClearPiece(theirPawn, (GetEpSquare(pos) + offset), pos);
		AddPiece(ourPawn, GetEpSquare(pos), pos);
		if (!((GetRookAttacks(kSQ, pos->occupancies[2]) &
			(GetPieceColorBB(pos, ROOK, color ^ 1) |
				GetPieceColorBB(pos, QUEEN, color ^ 1)))))
			moves |= (1ULL << GetEpSquare(pos));
		AddPiece(ourPawn, square, pos);
		AddPiece(theirPawn, GetEpSquare(pos) + offset, pos);
		ClearPiece(ourPawn, GetEpSquare(pos), pos);
	}
	return moves;
}

static inline Bitboard LegalKnightMoves(S_Board* pos, int color, int square) {
	if (pos->pinD & (1ULL << square) || pos->pinHV & (1ULL << square))
		return NOMOVE;
	return knight_attacks[square] & ~(Occupancy(pos, color)) &
		pos->checkMask;
}

static inline Bitboard LegalBishopMoves(S_Board* pos, int color, int square) {
	if (pos->pinHV & (1ULL << square))
		return NOMOVE;
	if (pos->pinD & (1ULL << square))
		return GetBishopAttacks(square, Occupancy(pos, BOTH)) &
		~(Occupancy(pos, color)) & pos->pinD & pos->checkMask;
	return GetBishopAttacks(square, Occupancy(pos, BOTH)) &
		~(Occupancy(pos, color)) & pos->checkMask;
}

static inline Bitboard LegalRookMoves(S_Board* pos, int color, int square) {
	if (pos->pinD & (1ULL << square))
		return NOMOVE;
	if (pos->pinHV & (1ULL << square))
		return GetRookAttacks(square, Occupancy(pos, BOTH)) &
		~(Occupancy(pos, color)) & pos->pinHV & pos->checkMask;
	return GetRookAttacks(square, Occupancy(pos, BOTH)) &
		~(Occupancy(pos, color)) & pos->checkMask;
}

static inline Bitboard LegalQueenMoves(S_Board* pos, int color, int square) {
	return LegalRookMoves(pos, color, square) |
		LegalBishopMoves(pos, color, square);
}

static inline Bitboard LegalKingMoves(S_Board* pos, int color, int square) {
	Bitboard moves = king_attacks[square] & ~Occupancy(pos, color);
	Bitboard final_moves = NOMOVE;
	int king = GetPiece(KING, color);
	ClearPiece(king, square, pos);
	while (moves) {
		int index = GetLsbIndex(moves);
		pop_bit(moves, index);
		if (IsSquareAttacked(pos, index, pos->side ^ 1)) {
			continue;
		}
		final_moves |= (1ULL << index);
	}
	AddPiece(king, square, pos);

	return final_moves;
}

// generate all moves
void GenerateMoves(S_MOVELIST* move_list, S_Board* pos) { // init move count
	move_list->count = 0;

	// define source & target squares
	int source_square, target_square;

	init(pos, pos->side, KingSQ(pos, pos->side));

	if (pos->checks < 2) {
		Bitboard pawns = GetPieceColorBB(pos, PAWN, pos->side);

		while (pawns) {
			// init source square
			source_square = GetLsbIndex(pawns);

			Bitboard moves = LegalPawnMoves(pos, pos->side, source_square);
			while (moves) {
				// init target square
				target_square = GetLsbIndex(moves);
				AddPawnMove(pos, source_square, target_square, move_list);
				pop_bit(moves, target_square);
			}

			// pop ls1b from piece bitboard copy
			pop_bit(pawns, source_square);
		}

		// genarate knight moves
		Bitboard knights = GetPieceColorBB(pos, KNIGHT, pos->side);

		while (knights) {
			source_square = GetLsbIndex(knights);
			Bitboard moves = LegalKnightMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(KNIGHT, pos->side);
				AddMove(
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(knights, source_square);
		}

		Bitboard bishops = GetPieceColorBB(pos, BISHOP, pos->side);

		while (bishops) {
			source_square = GetLsbIndex(bishops);
			Bitboard moves = LegalBishopMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(BISHOP, pos->side);
				AddMove(
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(bishops, source_square);
		}

		Bitboard rooks = GetPieceColorBB(pos, ROOK, pos->side);

		while (rooks) {
			source_square = GetLsbIndex(rooks);
			Bitboard moves = LegalRookMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(ROOK, pos->side);
				AddMove(
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(rooks, source_square);
		}

		Bitboard queens = GetPieceColorBB(pos, QUEEN, pos->side);
		while (queens) {
			source_square = GetLsbIndex(queens);
			Bitboard moves = LegalQueenMoves(pos, pos->side, source_square);

			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(QUEEN, pos->side);
				AddMove(
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
		target_square = GetLsbIndex(moves);
		int capture = PieceOn(pos, target_square) != EMPTY;

		pop_bit(moves, target_square);
		AddMove(
			encode_move(source_square, target_square, piece, 0, capture),
			move_list);
	}

	if (pos->checkMask == 18446744073709551615ULL) {
		if (pos->side == WHITE) {
			// king side castling is available
			if (pos->boardState.castleperm & WKCA) {
				// make sure square between king and king's rook are empty
				if (!get_bit(Occupancy(pos, BOTH), f1) &&
					!get_bit(Occupancy(pos, BOTH), g1)) {
					// make sure king and the f1 squares are not under attacks
					if (!IsSquareAttacked(pos, e1, BLACK) &&
						!IsSquareAttacked(pos, f1, BLACK) &&
						!IsSquareAttacked(pos, g1, BLACK))
						AddMove(encode_move(e1, g1, WK, 0, 0), move_list);
				}
			}

			if (pos->boardState.castleperm & WQCA) {
				// make sure square between king and queen's rook are empty
				if (!get_bit(Occupancy(pos, BOTH), d1) &&
					!get_bit(Occupancy(pos, BOTH), c1) &&
					!get_bit(Occupancy(pos, BOTH), b1)) {
					// make sure king and the d1 squares are not under attacks
					if (!IsSquareAttacked(pos, e1, BLACK) &&
						!IsSquareAttacked(pos, d1, BLACK) &&
						!IsSquareAttacked(pos, c1, BLACK))
						AddMove(encode_move(e1, c1, WK, 0, 0), move_list);
				}
			}
		}

		else {
			if (pos->boardState.castleperm & BKCA) {
				// make sure square between king and king's rook are empty
				if (!get_bit(Occupancy(pos, BOTH), f8) &&
					!get_bit(Occupancy(pos, BOTH), g8)) {
					// make sure king and the f8 squares are not under attacks
					if (!IsSquareAttacked(pos, e8, WHITE) &&
						!IsSquareAttacked(pos, f8, WHITE) &&
						!IsSquareAttacked(pos, g8, WHITE))
						AddMove(encode_move(e8, g8, BK, 0, 0), move_list);
				}
			}

			if (pos->boardState.castleperm & BQCA) {
				// make sure square between king and queen's rook are empty
				if (!get_bit(Occupancy(pos, BOTH), d8) &&
					!get_bit(Occupancy(pos, BOTH), c8) &&
					!get_bit(Occupancy(pos, BOTH), b8)) {
					// make sure king and the d8 squares are not under attacks
					if (!IsSquareAttacked(pos, e8, WHITE) &&
						!IsSquareAttacked(pos, d8, WHITE) &&
						!IsSquareAttacked(pos, c8, WHITE))
						AddMove(encode_move(e8, c8, BK, 0, 0), move_list);
				}
			}
		}
	}
}

// generate all moves
void GenerateCaptures(S_MOVELIST* move_list, S_Board* pos) {
	// init move count
	move_list->count = 0;

	// define source & target squares
	int source_square, target_square;

	init(pos, pos->side, KingSQ(pos, pos->side));

	if (pos->checks < 2) {
		Bitboard pawn_mask = GetPieceColorBB(pos, PAWN, pos->side);
		Bitboard knights_mask = GetPieceColorBB(pos, KNIGHT, pos->side);
		Bitboard bishops_mask = GetPieceColorBB(pos, BISHOP, pos->side);
		Bitboard rooks_mask = GetPieceColorBB(pos, ROOK, pos->side);
		Bitboard queens_mask = GetPieceColorBB(pos, QUEEN, pos->side);

		while (pawn_mask) {
			// init source square
			source_square = GetLsbIndex(pawn_mask);

			Bitboard moves =
				LegalPawnMoves(pos, pos->side, source_square) &
				(Enemy(pos) | 0xFF000000000000FF);
			while (moves) {
				// init target square
				target_square = GetLsbIndex(moves);
				AddPawnMove(pos, source_square, target_square, move_list);
				pop_bit(moves, target_square);
			}

			// pop ls1b from piece bitboard copy
			pop_bit(pawn_mask, source_square);
		}
		// genarate knight moves
		while (knights_mask) {
			source_square = GetLsbIndex(knights_mask);
			Bitboard moves = LegalKnightMoves(pos, pos->side, source_square) &
				(Enemy(pos));
			//while we have moves that the knight can play we add them to the list
			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(KNIGHT, pos->side);
				AddMove(
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(knights_mask, source_square);
		}

		while (bishops_mask) {
			source_square = GetLsbIndex(bishops_mask);
			Bitboard moves = LegalBishopMoves(pos, pos->side, source_square) &
				(Enemy(pos));

			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(BISHOP, pos->side);
				AddMove(
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(bishops_mask, source_square);
		}

		while (rooks_mask) {
			source_square = GetLsbIndex(rooks_mask);
			Bitboard moves = LegalRookMoves(pos, pos->side, source_square) &
				(pos->occupancies[pos->side ^ 1]);

			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(ROOK, pos->side);
				AddMove(
					encode_move(source_square, target_square, piece, 0, capture),
					move_list);
				pop_bit(moves, target_square);
			}

			pop_bit(rooks_mask, source_square);
		}

		while (queens_mask) {
			source_square = GetLsbIndex(queens_mask);
			Bitboard moves = LegalQueenMoves(pos, pos->side, source_square) &
				(Enemy(pos));

			while (moves) {
				target_square = GetLsbIndex(moves);
				int capture = PieceOn(pos, target_square) != EMPTY;
				int piece = GetPiece(QUEEN, pos->side);
				AddMove(
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
		(Enemy(pos));
	while (moves) {
		target_square = GetLsbIndex(moves);
		int capture = (pos->pieces[target_square] != EMPTY);

		pop_bit(moves, target_square);
		AddMove(
			encode_move(source_square, target_square, piece, 0, capture),
			move_list);
	}
}
