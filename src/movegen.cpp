#include "movegen.h"
#include "attack.h"
#include "init.h"
#include "magic.h"
#include "makemove.h"
#include "board.h"
#include "move.h"
#include "io.h"

// is the square given in input attacked by the current given side
bool IsSquareAttacked(const S_Board* pos, const Bitboard occ, const int square, const int side) {
    // is the square attacked by pawns
    if (pawn_attacks[side ^ 1][square] & pos->GetPieceColorBB(PAWN, side))
        return true;
    // is the square attacked by knights
    if (knight_attacks[square] & pos->GetPieceColorBB(KNIGHT, side))
        return true;
    // is the square attacked by kings
    if (king_attacks[square] & pos->GetPieceColorBB(KING, side))
        return true;
    // is the square attacked by bishops or queens
    if (GetBishopAttacks(square, occ) & (pos->GetPieceColorBB(BISHOP, side) | pos->GetPieceColorBB(QUEEN, side)))
        return true;
    // is the square attacked by rooks or queens
    if (GetRookAttacks(square, occ) & (pos->GetPieceColorBB(ROOK, side) | pos->GetPieceColorBB(QUEEN, side)))
        return true;
    // by default return false
    return false;
}

// is the square given in input attacked by the current given side
bool IsSquareAttacked(const S_Board* pos, const int square, const int side) {
    return IsSquareAttacked(pos, pos->Occupancy(BOTH), square, side);
}

static inline Bitboard PawnPush(int color, int sq) {
    return 1ULL << (sq + (color == WHITE ? -8 : 8));
}

static inline void init(S_Board* pos, int color, int sq) {
    Bitboard newMask = DoCheckmask(pos, color, sq);
    pos->checkMask = newMask ? newMask : 18446744073709551615ULL;
    DoPinMask(pos, color, sq);
}
// Check for move legality by generating the list of legal moves in a position and checking if that move is present
int MoveExists(S_Board* pos, const int move) {
    S_MOVELIST list[1];
    GenerateMoves(list, pos);

    for (int moveNum = 0; moveNum < list->count; ++moveNum) {
        if (list->moves[moveNum].move == move && IsLegal(pos, move)) {
            return true;
        }
    }
    return false;
}
// function that adds a move to the move list
static inline void AddMove(int move, S_MOVELIST* list) {
    list->moves[list->count].move = move;
    list->moves[list->count].score = 0;
    list->count++;
}
// function that adds a pawn move (and all its possible branches) to the move list
static inline void AddPawnMove(const S_Board* pos, const int from, const int to, S_MOVELIST* list) {
    Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
    if (!(abs(to - from) - 16)) movetype = Movetype::doublePush;
    else if(!(to - pos->enPas)) movetype = Movetype::enPassant;

    if (pos->side == WHITE) {
        if (from >= a7 &&
            from <= h7) { // if the piece is moving from the 7th to the 8th rank
            AddMove(encode_move(from, to, WP, (Movetype::queenPromo | movetype)), list);
            AddMove(encode_move(from, to, WP, (Movetype::rookPromo | movetype)), list); // consider every possible piece promotion
            AddMove(encode_move(from, to, WP, (Movetype::bishopPromo | movetype)), list);
            AddMove(encode_move(from, to, WP, (Movetype::knightPromo | movetype)), list);
        }
        else { // else do not include possible promotions
            AddMove(encode_move(from, to, WP,  movetype), list);
        }
    }

    else {
        if (from >= a2 &&
            from <= h2) { // if the piece is moving from the 2nd to the 1st rank
            AddMove(encode_move(from, to, BP, (Movetype::queenPromo | movetype)), list);
            AddMove(encode_move(from, to, BP, (Movetype::rookPromo | movetype)), list); // consider every possible piece promotion
            AddMove(encode_move(from, to, BP, (Movetype::bishopPromo | movetype)), list);
            AddMove(encode_move(from, to, BP, (Movetype::knightPromo | movetype)), list);
        }
        else { // else do not include possible promotions
            AddMove(encode_move(from, to, BP, movetype), list);
        }
    }
}

static inline Bitboard PseudoLegalPawnMoves(S_Board* pos, int color, int square) {

    const Bitboard enemy = pos->occupancies[color ^ 1];
    // Calculate pawn pushs
    Bitboard push = PawnPush(color, square) & ~pos->Occupancy(BOTH);

    push |=
        (color == WHITE)
        ? (get_rank[square] == 1 ? (push >> 8) & ~pos->Occupancy(BOTH) : 0ULL)
        : (get_rank[square] == 6 ? (push << 8) & ~pos->Occupancy(BOTH) : 0ULL);

    int offset = color * -16 + 8;
    Bitboard attacks = pawn_attacks[color][square];
    // If we are in check and the en passant square lies on our attackmask and
    // the en passant piece gives check return the ep mask as a move square
    if (pos->checkers && GetEpSquare(pos) != no_sq &&
        attacks & (1ULL << GetEpSquare(pos)) &&
        pos->checkMask & (1ULL << (GetEpSquare(pos) + offset)))
        return (attacks & (1ULL << GetEpSquare(pos)));

    // If we are in check we can do all moves that are on the checkmask
    if (pos->checkers)
        return ((attacks & enemy) | push) & pos->checkMask;

    Bitboard moves = ((attacks & enemy) | push);

    if (GetEpSquare(pos) != no_sq && SquareDistance(square, GetEpSquare(pos)) == 1 &&
        (1ULL << GetEpSquare(pos)) & attacks) {
        moves |= (1ULL << GetEpSquare(pos));
    }
    return moves;
}

static inline Bitboard PseudoLegalKnightMoves(S_Board* pos, int color, int square) {
    return knight_attacks[square] & ~pos->Occupancy(color) & pos->checkMask;
}

static inline Bitboard PseudoLegalBishopMoves(S_Board* pos, int color, int square) {
    return GetBishopAttacks(square, pos->Occupancy(BOTH)) & ~(pos->Occupancy(color)) & pos->checkMask;
}

static inline Bitboard PseudoLegalRookMoves(S_Board* pos, int color, int square) {
    return GetRookAttacks(square, pos->Occupancy(BOTH)) & ~(pos->Occupancy(color)) & pos->checkMask;
}

static inline Bitboard PseudoLegalQueenMoves(S_Board* pos, int color, int square) {
    return (PseudoLegalRookMoves(pos, color, square) | PseudoLegalBishopMoves(pos, color, square)) & pos->checkMask;
}

static inline Bitboard PseudoLegalKingMoves(S_Board* pos, int color, int square) {
    return king_attacks[square] & ~pos->Occupancy(color);
}

// generate all moves
void GenerateMoves(S_MOVELIST* move_list, S_Board* pos) { // init move count
    move_list->count = 0;

    // define source & target squares
    int sourceSquare, targetSquare;

    init(pos, pos->side, KingSQ(pos, pos->side));

    if (pos->checks < 2) {
        Bitboard pawns = pos->GetPieceColorBB(PAWN, pos->side);
        while (pawns) {
            // init source square
            sourceSquare = GetLsbIndex(pawns);
            Bitboard moves = PseudoLegalPawnMoves(pos, pos->side, sourceSquare);
            while (moves) {
                // init target square
                targetSquare = GetLsbIndex(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
                pop_bit(moves, targetSquare);
            }
            // pop lsb from piece bitboard copy
            pop_bit(pawns, sourceSquare);
        }

        // genarate knight moves
        Bitboard knights = pos->GetPieceColorBB(KNIGHT, pos->side);
        while (knights) {
            sourceSquare = GetLsbIndex(knights);
            Bitboard moves = PseudoLegalKnightMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(KNIGHT, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(knights, sourceSquare);
        }

        Bitboard bishops = pos->GetPieceColorBB(BISHOP, pos->side);
        while (bishops) {
            sourceSquare = GetLsbIndex(bishops);
            Bitboard moves = PseudoLegalBishopMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(BISHOP, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(bishops, sourceSquare);
        }

        Bitboard rooks = pos->GetPieceColorBB(ROOK, pos->side);
        while (rooks) {
            sourceSquare = GetLsbIndex(rooks);
            Bitboard moves = PseudoLegalRookMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(ROOK, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(rooks, sourceSquare);
        }

        Bitboard queens = pos->GetPieceColorBB(QUEEN, pos->side);
        while (queens) {
            sourceSquare = GetLsbIndex(queens);
            Bitboard moves = PseudoLegalQueenMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(QUEEN, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(queens, sourceSquare);
        }
    }

    sourceSquare = KingSQ(pos, pos->side);
    const int piece = GetPiece(KING, pos->side);
    Bitboard moves = PseudoLegalKingMoves(pos, pos->side, sourceSquare);
    while (moves) {
        targetSquare = GetLsbIndex(moves);
        Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
        AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
        pop_bit(moves, targetSquare);
    }

    if (!pos->checkers) {
        if (pos->side == WHITE) {
            // king side castling is available
            if (pos->GetCastlingPerm() & WKCA) {
                // make sure square between king and king's rook are empty
                if (!get_bit(pos->Occupancy(BOTH), f1) &&
                    !get_bit(pos->Occupancy(BOTH), g1)) {
                    // make sure king and the f1 squares are not under attacks
                    if (!IsSquareAttacked(pos, e1, BLACK) &&
                        !IsSquareAttacked(pos, f1, BLACK) &&
                        !IsSquareAttacked(pos, g1, BLACK))
                        AddMove(encode_move(e1, g1, WK, Movetype::KSCastle), move_list);
                }
            }

            if (pos->GetCastlingPerm() & WQCA) {
                // make sure square between king and queen's rook are empty
                if (!get_bit(pos->Occupancy(BOTH), d1) &&
                    !get_bit(pos->Occupancy(BOTH), c1) &&
                    !get_bit(pos->Occupancy(BOTH), b1)) {
                    // make sure king and the d1 squares are not under attacks
                    if (!IsSquareAttacked(pos, e1, BLACK) &&
                        !IsSquareAttacked(pos, d1, BLACK) &&
                        !IsSquareAttacked(pos, c1, BLACK))
                        AddMove(encode_move(e1, c1, WK, Movetype::QSCastle), move_list);
                }
            }
        }

        else {
            if (pos->GetCastlingPerm() & BKCA) {
                // make sure square between king and king's rook are empty
                if (!get_bit(pos->Occupancy(BOTH), f8) &&
                    !get_bit(pos->Occupancy(BOTH), g8)) {
                    // make sure king and the f8 squares are not under attacks
                    if (!IsSquareAttacked(pos, e8, WHITE) &&
                        !IsSquareAttacked(pos, f8, WHITE) &&
                        !IsSquareAttacked(pos, g8, WHITE))
                        AddMove(encode_move(e8, g8, BK, Movetype::KSCastle), move_list);
                }
            }

            if (pos->GetCastlingPerm() & BQCA) {
                // make sure square between king and queen's rook are empty
                if (!get_bit(pos->Occupancy(BOTH), d8) &&
                    !get_bit(pos->Occupancy(BOTH), c8) &&
                    !get_bit(pos->Occupancy(BOTH), b8)) {
                    // make sure king and the d8 squares are not under attacks
                    if (!IsSquareAttacked(pos, e8, WHITE) &&
                        !IsSquareAttacked(pos, d8, WHITE) &&
                        !IsSquareAttacked(pos, c8, WHITE))
                        AddMove(encode_move(e8, c8, BK, Movetype::QSCastle), move_list);
                }
            }
        }
    }
}

// generate all captures
void GenerateCaptures(S_MOVELIST* move_list, S_Board* pos) {
    // init move count
    move_list->count = 0;

    // define source & target squares
    int sourceSquare, targetSquare;

    init(pos, pos->side, KingSQ(pos, pos->side));

    if (pos->checks < 2) {
        Bitboard pawn_mask = pos->GetPieceColorBB(PAWN, pos->side);
        Bitboard knights_mask = pos->GetPieceColorBB(KNIGHT, pos->side);
        Bitboard bishops_mask = pos->GetPieceColorBB(BISHOP, pos->side);
        Bitboard rooks_mask = pos->GetPieceColorBB(ROOK, pos->side);
        Bitboard queens_mask = pos->GetPieceColorBB(QUEEN, pos->side);

        while (pawn_mask) {
            // init source square
            sourceSquare = GetLsbIndex(pawn_mask);
            Bitboard moves = PseudoLegalPawnMoves(pos, pos->side, sourceSquare) & (pos->Enemy() | 0xFF000000000000FF);

            while (moves) {
                // init target square
                targetSquare = GetLsbIndex(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
                pop_bit(moves, targetSquare);
            }

            // pop lsb from piece bitboard copy
            pop_bit(pawn_mask, sourceSquare);
        }
        // genarate knight moves
        while (knights_mask) {
            sourceSquare = GetLsbIndex(knights_mask);
            Bitboard moves = PseudoLegalKnightMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(KNIGHT, pos->side);
            // while we have moves that the knight can play we add them to the list
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(knights_mask, sourceSquare);
        }

        while (bishops_mask) {
            sourceSquare = GetLsbIndex(bishops_mask);
            Bitboard moves = PseudoLegalBishopMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(BISHOP, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(bishops_mask, sourceSquare);
        }

        while (rooks_mask) {
            sourceSquare = GetLsbIndex(rooks_mask);
            Bitboard moves = PseudoLegalRookMoves(pos, pos->side, sourceSquare) &
                (pos->Occupancy(pos->side ^ 1));
            const int piece = GetPiece(ROOK, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(rooks_mask, sourceSquare);
        }

        while (queens_mask) {
            sourceSquare = GetLsbIndex(queens_mask);
            Bitboard moves = PseudoLegalQueenMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(QUEEN, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(queens_mask, sourceSquare);
        }
    }

    sourceSquare = KingSQ(pos, pos->side);
    const int piece = GetPiece(KING, pos->side);
    Bitboard king_moves = PseudoLegalKingMoves(pos, pos->side, sourceSquare) & pos->Enemy();

    while (king_moves) {
        targetSquare = GetLsbIndex(king_moves);
        pop_bit(king_moves, targetSquare);
        AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
    }
}

bool IsLegal(const S_Board* pos, const int move) {

    if (IsCastle(move))
        return true;

    int from = From(move);
    int to = To(move);
    int ksq = KingSQ(pos, pos->side);

    if (isEnpassant(move)) {
        const int NORTH = pos->side == WHITE ? 8 : -8;
        Bitboard occ = pos->Occupancy(BOTH) ^ (1ULL << from) ^ (1ULL << (to+NORTH)) ^ (1ULL << to);
        return !IsSquareAttacked(pos, occ, ksq, pos->side ^ 1);
    }

    if (GetPieceType(Piece(move)) == KING)
        return !IsSquareAttacked(pos, pos->Occupancy(BOTH) ^ (1ULL << from), to, pos->side ^ 1);

    Bitboard pins = pos->pinHV | pos->pinD;
    return !(pins & (1ULL << from)) || (get_file[from] == get_file[ksq] && get_file[to] == get_file[ksq]) 
                                    || (get_rank[from] == get_rank[ksq] && get_rank[to] == get_rank[ksq])
                                    || (get_diagonal[from] == get_diagonal[ksq] && get_diagonal[to] == get_diagonal[ksq])
                                    || (get_antidiagonal(from) == get_antidiagonal(ksq) && get_antidiagonal(to) == get_antidiagonal(ksq));
}
