#include "movegen.h"
#include "attack.h"
#include "init.h"
#include "magic.h"
#include "makemove.h"
#include "board.h"
#include "move.h"
#include <iostream>
#include "io.h"

// is the square given in input attacked by the current given side
bool IsSquareAttacked(const S_Board* pos, const int square, const int side) {
    // Take the occupancies of both positions, encoding where all the pieces on the board reside
    Bitboard occ = pos->Occupancy(BOTH);
    // is the square attacked by pawns
    if (pawn_attacks[side ^ 1][square] & pos->GetPieceColorBB(PAWN, side))
        return true;
    // is the square attacked by knights
    if (knight_attacks[square] & pos->GetPieceColorBB(KNIGHT, side))
        return true;
    // is the square attacked by kings
    if (king_attacks[square] & pos->GetPieceColorBB(KING, side))
        return true;
    // is the square attacked by bishops
    if (GetBishopAttacks(square, occ) & (pos->GetPieceColorBB(BISHOP, side) | pos->GetPieceColorBB(QUEEN, side)))
        return true;
    // is the square attacked by rooks
    if (GetRookAttacks(square, occ) & (pos->GetPieceColorBB(ROOK, side) | pos->GetPieceColorBB(QUEEN, side)))
        return true;
    // by default return false
    return false;
}

static inline Bitboard PawnPush(int color, int sq) {
    if (color == WHITE)
        return (1ULL << (sq - 8));
    return (1ULL << (sq + 8));
}

static inline void init(S_Board* pos, int color, int sq) {
    DoPinMask(pos, color, sq);
}
// Check for move legality by generating the list of legal moves in a position and checking if that move is present
int MoveExists(S_Board* pos, const int move) {
    S_MOVELIST list[1];
    GenerateMoves(list, pos);

    for (int moveNum = 0; moveNum < list->count; ++moveNum) {
        if (list->moves[moveNum].move == move) {
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

static inline Bitboard LegalPawnMoves(S_Board* pos, int color, int square, Bitboard checkMask) {
    const Bitboard enemy = pos->occupancies[color ^ 1];

    // If we are pinned diagonally we can only do captures which are on the pin_dg
    // and on the checkmask

    if (pos->pinD & (1ULL << square))
        return pawn_attacks[color][square] & pos->pinD & checkMask & (enemy | (1ULL << GetEpSquare(pos)));
    // Calculate pawn pushs
    Bitboard push = PawnPush(color, square) & ~pos->Occupancy(BOTH);

    push |=
        (color == WHITE)
        ? (get_rank[square] == 1 ? (push >> 8) & ~pos->Occupancy(BOTH) : 0ULL)
        : (get_rank[square] == 6 ? (push << 8) & ~pos->Occupancy(BOTH) : 0ULL);

    // If we are pinned horizontally we can do no moves but if we are pinned
    // vertically we can only do pawn pushs
    if (pos->pinHV & (1ULL << square))
        return push & pos->pinHV & checkMask;
    int offset = color * -16 + 8;
    Bitboard attacks = pawn_attacks[color][square];
    // If we are in check and  the en passant square lies on our attackmask and
    // the en passant piece gives check return the ep mask as a move square
    if (checkMask != 18446744073709551615ULL && GetEpSquare(pos) != no_sq &&
        attacks & (1ULL << GetEpSquare(pos)) &&
        checkMask & (1ULL << (GetEpSquare(pos) + offset)))
        return (attacks & (1ULL << GetEpSquare(pos)));
    // If we are in check we can do all moves that are on the checkmask
    if (checkMask != 18446744073709551615ULL)
        return ((attacks & enemy) | push) & checkMask;

    Bitboard moves = ((attacks & enemy) | push) & checkMask;

    if (GetEpSquare(pos) != no_sq && SquareDistance(square, GetEpSquare(pos)) == 1 &&
        (1ULL << GetEpSquare(pos)) & attacks) {
        int ourPawn = GetPiece(PAWN, color);
        int theirPawn = GetPiece(PAWN, color ^ 1);
        int kSQ = KingSQ(pos, color);
        ClearPiece(ourPawn, square, pos);
        ClearPiece(theirPawn, (GetEpSquare(pos) + offset), pos);
        AddPiece(ourPawn, GetEpSquare(pos), pos);
        if (!((GetRookAttacks(kSQ, pos->Occupancy(BOTH)) &
            (pos->GetPieceColorBB(ROOK, color ^ 1) |
                pos->GetPieceColorBB(QUEEN, color ^ 1)))))
            moves |= (1ULL << GetEpSquare(pos));
        AddPiece(ourPawn, square, pos);
        AddPiece(theirPawn, GetEpSquare(pos) + offset, pos);
        ClearPiece(ourPawn, GetEpSquare(pos), pos);
    }
    return moves;
}

static inline Bitboard LegalKnightMoves(S_Board* pos, int color, int square, Bitboard checkMask) {
    if (pos->pinD & (1ULL << square) || pos->pinHV & (1ULL << square))
        return NOMOVE;
    return knight_attacks[square] & ~pos->Occupancy(color) &
        checkMask;
}

static inline Bitboard LegalBishopMoves(S_Board* pos, int color, int square, Bitboard checkMask) {
    if (pos->pinHV & (1ULL << square))
        return NOMOVE;
    if (pos->pinD & (1ULL << square))
        return GetBishopAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & pos->pinD & checkMask;
    return GetBishopAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & checkMask;
}

static inline Bitboard LegalRookMoves(S_Board* pos, int color, int square, Bitboard checkMask) {
    if (pos->pinD & (1ULL << square))
        return NOMOVE;
    if (pos->pinHV & (1ULL << square))
        return GetRookAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & pos->pinHV & checkMask;
    return GetRookAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & checkMask;
}

static inline Bitboard LegalQueenMoves(S_Board* pos, int color, int square, Bitboard checkMask) {
    return LegalRookMoves(pos, color, square, checkMask) |
        LegalBishopMoves(pos, color, square, checkMask);
}

static inline Bitboard LegalKingMoves(S_Board* pos, int color, int square) {
    Bitboard moves = king_attacks[square] & ~pos->Occupancy(color);
    Bitboard finalMoves = NOMOVE;
    int king = GetPiece(KING, color);
    ClearPiece(king, square, pos);
    while (moves) {
        int index = GetLsbIndex(moves);
        pop_bit(moves, index);
        if (IsSquareAttacked(pos, index, pos->side ^ 1)) {
            continue;
        }
        finalMoves |= (1ULL << index);
    }
    AddPiece(king, square, pos);

    return finalMoves;
}

// generate all moves
void GenerateMoves(S_MOVELIST* move_list, S_Board* pos) { // init move count
    move_list->count = 0;

    // define source & target squares
    int sourceSquare, targetSquare;

    init(pos, pos->side, KingSQ(pos, pos->side));

    const int checks = CountBits(pos->checkers);

    if (checks < 2) {
        // If we are in check get the squares between the checking piece and the king
        Bitboard checkMask = fullCheckmask;
        if (pos->checkers) {
            checkMask = 0;
            const int kingSquare = KingSQ(pos, pos->side);
            Bitboard checkingPieces = pos->checkers;
            const int pieceLocation = GetLsbIndex(checkingPieces);
            checkMask |= (1ULL << pieceLocation) | SQUARES_BETWEEN_BB[pieceLocation][kingSquare];
        }

        Bitboard pawns = pos->GetPieceColorBB(PAWN, pos->side);
        while (pawns) {
            // init source square
            sourceSquare = GetLsbIndex(pawns);
            Bitboard moves = LegalPawnMoves(pos, pos->side, sourceSquare, checkMask);
            while (moves) {
                // init target square
                targetSquare = GetLsbIndex(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
                pop_bit(moves, targetSquare);
            }
            // pop ls1b from piece bitboard copy
            pop_bit(pawns, sourceSquare);
        }

        // genarate knight moves
        Bitboard knights = pos->GetPieceColorBB(KNIGHT, pos->side);
        while (knights) {
            sourceSquare = GetLsbIndex(knights);
            Bitboard moves = LegalKnightMoves(pos, pos->side, sourceSquare, checkMask);
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
            Bitboard moves = LegalBishopMoves(pos, pos->side, sourceSquare, checkMask);
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
            Bitboard moves = LegalRookMoves(pos, pos->side, sourceSquare, checkMask);
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
            Bitboard moves = LegalQueenMoves(pos, pos->side, sourceSquare, checkMask);
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
    Bitboard moves = LegalKingMoves(pos, pos->side, sourceSquare);
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

// generate all moves
void GenerateCaptures(S_MOVELIST* move_list, S_Board* pos) {
    // init move count
    move_list->count = 0;

    // define source & target squares
    int sourceSquare, targetSquare;

    init(pos, pos->side, KingSQ(pos, pos->side));

    const int checks = CountBits(pos->checkers);

    if (checks < 2) {

        // If we are in check get the squares between the checking piece and the king
        Bitboard checkMask = fullCheckmask;
        if (pos->checkers) {
            checkMask = 0;
            const int kingSquare = KingSQ(pos, pos->side);
            Bitboard checkingPieces = pos->checkers;
            const int pieceLocation = GetLsbIndex(checkingPieces);
            checkMask |= (1ULL << pieceLocation) | SQUARES_BETWEEN_BB[pieceLocation][kingSquare];
        }

        Bitboard pawn_mask = pos->GetPieceColorBB(PAWN, pos->side);

        while (pawn_mask) {
            // init source square
            sourceSquare = GetLsbIndex(pawn_mask);
            Bitboard moves = LegalPawnMoves(pos, pos->side, sourceSquare, checkMask) & (pos->Enemy() | 0xFF000000000000FF);

            while (moves) {
                // init target square
                targetSquare = GetLsbIndex(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
                pop_bit(moves, targetSquare);
            }

            // pop ls1b from piece bitboard copy
            pop_bit(pawn_mask, sourceSquare);
        }
        // genarate knight moves
        Bitboard knights_mask = pos->GetPieceColorBB(KNIGHT, pos->side);
        while (knights_mask) {
            sourceSquare = GetLsbIndex(knights_mask);
            Bitboard moves = LegalKnightMoves(pos, pos->side, sourceSquare, checkMask) & pos->Enemy();
            const int piece = GetPiece(KNIGHT, pos->side);
            // while we have moves that the knight can play we add them to the list
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(knights_mask, sourceSquare);
        }
        Bitboard bishops_mask = pos->GetPieceColorBB(BISHOP, pos->side);
        while (bishops_mask) {
            sourceSquare = GetLsbIndex(bishops_mask);
            Bitboard moves = LegalBishopMoves(pos, pos->side, sourceSquare, checkMask) & pos->Enemy();
            const int piece = GetPiece(BISHOP, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(bishops_mask, sourceSquare);
        }
        Bitboard rooks_mask = pos->GetPieceColorBB(ROOK, pos->side);
        while (rooks_mask) {
            sourceSquare = GetLsbIndex(rooks_mask);
            Bitboard moves = LegalRookMoves(pos, pos->side, sourceSquare, checkMask) &
                (pos->Occupancy(pos->side ^ 1));
            const int piece = GetPiece(ROOK, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_bit(moves, targetSquare);
            }
            pop_bit(rooks_mask, sourceSquare);
        }
        Bitboard queens_mask = pos->GetPieceColorBB(QUEEN, pos->side);
        while (queens_mask) {
            sourceSquare = GetLsbIndex(queens_mask);
            Bitboard moves = LegalQueenMoves(pos, pos->side, sourceSquare, checkMask) & pos->Enemy();
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
    Bitboard king_moves = LegalKingMoves(pos, pos->side, sourceSquare) & pos->Enemy();

    while (king_moves) {
        targetSquare = GetLsbIndex(king_moves);
        pop_bit(king_moves, targetSquare);
        AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
    }
}
