#include "movegen.h"
#include "attack.h"
#include "init.h"
#include "magic.h"
#include "makemove.h"
#include "board.h"
#include "move.h"

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

// Check for move legality by generating the list of legal moves in a position and checking if that move is present
bool MoveExists(S_Board* pos, const int move) {
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
    else if(to == GetEpSquare(pos)) movetype = Movetype::enPassant;

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

static inline Bitboard LegalPawnMoves(S_Board* pos, int color, int square) {
    const Bitboard enemy = pos->Enemy();

    // If we are pinned diagonally we can only do captures which are on the pin_dg
    // and on the checkmask

    if (pos->boardState.pinD & (1ULL << square))
        return pawn_attacks[color][square] & pos->boardState.pinD & pos->boardState.checkMask & (enemy | (1ULL << GetEpSquare(pos)));
    // Calculate pawn pushs
    Bitboard push = PawnPush(color, square) & ~pos->Occupancy(BOTH);

    push |=
        (color == WHITE)
        ? (get_rank[square] == 1 ? (push >> 8) & ~pos->Occupancy(BOTH) : 0ULL)
        : (get_rank[square] == 6 ? (push << 8) & ~pos->Occupancy(BOTH) : 0ULL);

    // If we are pinned horizontally we can do no moves but if we are pinned
    // vertically we can only do pawn pushs
    if (pos->boardState.pinHV & (1ULL << square))
        return push & pos->boardState.pinHV & pos->boardState.checkMask;
    int offset = color * -16 + 8;
    Bitboard attacks = pawn_attacks[color][square];
    // If we are in check and  the en passant square lies on our attackmask and
    // the en passant piece gives check return the ep mask as a move square
    if (pos->boardState.checkers && GetEpSquare(pos) != no_sq &&
        attacks & (1ULL << GetEpSquare(pos)) &&
        pos->boardState.checkMask & (1ULL << (GetEpSquare(pos) + offset)))
        return (attacks & (1ULL << GetEpSquare(pos)));
    // If we are in check we can do all moves that are on the checkmask
    if (pos->boardState.checkers)
        return ((attacks & enemy) | push) & pos->boardState.checkMask;

    Bitboard moves = ((attacks & enemy) | push) & pos->boardState.checkMask;

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

static inline Bitboard LegalKnightMoves(S_Board* pos, int color, int square) {
    if (pos->boardState.pinD & (1ULL << square) || pos->boardState.pinHV & (1ULL << square))
        return NOMOVE;
    return knight_attacks[square] & ~pos->Occupancy(color) &
        pos->boardState.checkMask;
}

static inline Bitboard LegalBishopMoves(S_Board* pos, int color, int square) {
    if (pos->boardState.pinHV & (1ULL << square))
        return NOMOVE;
    if (pos->boardState.pinD & (1ULL << square))
        return GetBishopAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & pos->boardState.pinD & pos->boardState.checkMask;
    return GetBishopAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & pos->boardState.checkMask;
}

static inline Bitboard LegalRookMoves(S_Board* pos, int color, int square) {
    if (pos->boardState.pinD & (1ULL << square))
        return NOMOVE;
    if (pos->boardState.pinHV & (1ULL << square))
        return GetRookAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & pos->boardState.pinHV & pos->boardState.checkMask;
    return GetRookAttacks(square, pos->Occupancy(BOTH)) &
        ~(pos->Occupancy(color)) & pos->boardState.checkMask;
}

static inline Bitboard LegalQueenMoves(S_Board* pos, int color, int square) {
    return LegalRookMoves(pos, color, square) |
        LegalBishopMoves(pos, color, square);
}

static inline Bitboard LegalKingMoves(S_Board* pos, int color, int square) {
    Bitboard moves = king_attacks[square] & ~pos->Occupancy(color);
    Bitboard finalMoves = NOMOVE;
    int king = GetPiece(KING, color);
    ClearPiece(king, square, pos);
    while (moves) {
        int index = GetLsbIndex(moves);
        pop_lsb(moves);
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

    const int checks = CountBits(pos->boardState.checkers);
    
    if (checks < 2) {
        Bitboard pawns = pos->GetPieceColorBB(PAWN, pos->side);
        while (pawns) {
            // init source square
            sourceSquare = GetLsbIndex(pawns);
            Bitboard moves = LegalPawnMoves(pos, pos->side, sourceSquare);
            while (moves) {
                // init target square
                targetSquare = GetLsbIndex(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
                pop_lsb(moves);
            }
            // pop lsb from piece bitboard copy
            pop_lsb(pawns);
        }

        // genarate knight moves
        Bitboard knights = pos->GetPieceColorBB(KNIGHT, pos->side);
        while (knights) {
            sourceSquare = GetLsbIndex(knights);
            Bitboard moves = LegalKnightMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(KNIGHT, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_lsb(moves);
            }
            pop_lsb(knights);
        }

        Bitboard bishops = pos->GetPieceColorBB(BISHOP, pos->side);
        while (bishops) {
            sourceSquare = GetLsbIndex(bishops);
            Bitboard moves = LegalBishopMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(BISHOP, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_lsb(moves);
            }
            pop_lsb(bishops);
        }

        Bitboard rooks = pos->GetPieceColorBB(ROOK, pos->side);
        while (rooks) {
            sourceSquare = GetLsbIndex(rooks);
            Bitboard moves = LegalRookMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(ROOK, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_lsb(moves);
            }
            pop_lsb(rooks);
        }

        Bitboard queens = pos->GetPieceColorBB(QUEEN, pos->side);
        while (queens) {
            sourceSquare = GetLsbIndex(queens);
            Bitboard moves = LegalQueenMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(QUEEN, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
                pop_lsb(moves);
            }
            pop_lsb(queens);
        }
    }

    sourceSquare = KingSQ(pos, pos->side);
    const int piece = GetPiece(KING, pos->side);
    Bitboard moves = LegalKingMoves(pos, pos->side, sourceSquare);
    while (moves) {
        targetSquare = GetLsbIndex(moves);
        Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
        AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
        pop_lsb(moves);
    }

    if (!pos->boardState.checkers) {
        if (pos->side == WHITE) {
            // king side castling is available
            if (pos->GetCastlingPerm() & WKCA) {
                // make sure square between king and king's rook are empty
                if (!get_bit(pos->Occupancy(BOTH), f1) &&
                    !get_bit(pos->Occupancy(BOTH), g1)) {
                    // make sure king and the f1 squares are not under attacks
                    if (!IsSquareAttacked(pos, f1, BLACK) &&
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
                    if (!IsSquareAttacked(pos, d1, BLACK) &&
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
                    if (!IsSquareAttacked(pos, f8, WHITE) &&
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
                    if (!IsSquareAttacked(pos, d8, WHITE) &&
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

    const int checks = CountBits(pos->boardState.checkers);

    if (checks < 2) {

        Bitboard pawn_mask = pos->GetPieceColorBB(PAWN, pos->side);

        while (pawn_mask) {
            // init source square
            sourceSquare = GetLsbIndex(pawn_mask);
            Bitboard moves = LegalPawnMoves(pos, pos->side, sourceSquare) & (pos->Enemy() | 0xFF000000000000FF | (1ULL << GetEpSquare(pos)));
            while (moves) {
                // init target square
                targetSquare = GetLsbIndex(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
                pop_lsb(moves);
            }
            // pop lsb from piece bitboard copy
            pop_lsb(pawn_mask);
        }
        // genarate knight moves
        Bitboard knights_mask = pos->GetPieceColorBB(KNIGHT, pos->side);
        while (knights_mask) {
            sourceSquare = GetLsbIndex(knights_mask);
            Bitboard moves = LegalKnightMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(KNIGHT, pos->side);
            // while we have moves that the knight can play we add them to the list
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_lsb(moves);
            }
            pop_lsb(knights_mask);
        }
        Bitboard bishops_mask = pos->GetPieceColorBB(BISHOP, pos->side);
        while (bishops_mask) {
            sourceSquare = GetLsbIndex(bishops_mask);
            Bitboard moves = LegalBishopMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(BISHOP, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_lsb(moves);
            }
            pop_lsb(bishops_mask);
        }
        Bitboard rooks_mask = pos->GetPieceColorBB(ROOK, pos->side);
        while (rooks_mask) {
            sourceSquare = GetLsbIndex(rooks_mask);
            Bitboard moves = LegalRookMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(ROOK, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_lsb(moves);
            }
            pop_lsb(rooks_mask);
        }
        Bitboard queens_mask = pos->GetPieceColorBB(QUEEN, pos->side);
        while (queens_mask) {
            sourceSquare = GetLsbIndex(queens_mask);
            Bitboard moves = LegalQueenMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(QUEEN, pos->side);
            while (moves) {
                targetSquare = GetLsbIndex(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
                pop_lsb(moves);
            }
            pop_lsb(queens_mask);
        }
    }

    sourceSquare = KingSQ(pos, pos->side);
    const int piece = GetPiece(KING, pos->side);
    Bitboard king_moves = LegalKingMoves(pos, pos->side, sourceSquare) & pos->Enemy();

    while (king_moves) {
        targetSquare = GetLsbIndex(king_moves);
        pop_lsb(king_moves);
        AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
    }
}

// Pseudo-legality test inspired by Koivisto
bool IsPseudoLegal(S_Board* pos, int move) {

    if (move == NOMOVE)
        return false;

    const int from = From(move);
    const int to = To(move);
    const int movedPiece = Piece(move);
    const int pieceType = GetPieceType(movedPiece);

    if (from == to)
        return false;

    if (movedPiece == EMPTY)
        return false;

    if (pos->PieceOn(from) != movedPiece)
        return false;

    if (Color[movedPiece] != pos->side)
        return false;

    if ((1ULL << to) & pos->Occupancy(pos->side))
        return false;

    if ((!isCapture(move) || isEnpassant(move)) && pos->PieceOn(to) != EMPTY)
        return false;

    if (isCapture(move) && !isEnpassant(move) && pos->PieceOn(to) == EMPTY)
        return false;

    if ((   isDP(move)
         || isPromo(move)
         || isEnpassant(move)) && pieceType != PAWN)
        return false;

    if (isCastle(move) && pieceType != KING)
        return false;

    if ((CountBits(pos->boardState.checkers) >= 2) && pieceType != KING)
        return false;

    int NORTH = pos->side == WHITE ? -8 : 8;

    switch (pieceType) {
        case PAWN:
            if (isDP(move)) {
                if (from + NORTH + NORTH != to)
                    return false;

                if (pos->PieceOn(from + NORTH) != EMPTY)
                    return false;

                if (   (pos->side == WHITE && get_rank[from] != 1)
                    || (pos->side == BLACK && get_rank[from] != 6))
                    return false;
            }
            else if (!isCapture(move)) {
                if (from + NORTH != to)
                    return false;
            }
            if (isEnpassant(move)) {
                if (to != GetEpSquare(pos))
                    return false;

                if (!((1ULL << (to - NORTH)) & pos->GetPieceColorBB(PAWN, pos->side ^ 1)))
                    return false;
            }
            if (isCapture(move) && !(pawn_attacks[pos->side][from] & (1ULL << to)))
                return false;

            if (isPromo(move)) {
                if (   (pos->side == WHITE && get_rank[from] != 6)
                    || (pos->side == BLACK && get_rank[from] != 1))
                    return false;

                if (   (pos->side == WHITE && get_rank[to] != 7)
                    || (pos->side == BLACK && get_rank[to] != 0))
                    return false;
            }
            else {
                if (   (pos->side == WHITE && get_rank[from] >= 6)
                    || (pos->side == BLACK && get_rank[from] <= 1))
                    return false;
            }
            break;

        case KNIGHT:
            if (!(knight_attacks[from] & (1ULL << to)))
                return false;

            break;

        case BISHOP:
            if (   get_diagonal[from] != get_diagonal[to]
                && get_antidiagonal(from) != get_antidiagonal(to))
                return false;

            if (RayBetween(from, to) & pos->Occupancy(BOTH))
                return false;

            break;

        case ROOK:
            if (   get_file[from] != get_file[to]
                && get_rank[from] != get_rank[to])
                return false;

            if (RayBetween(from, to) & pos->Occupancy(BOTH))
                return false;

            break;

        case QUEEN:
            if (   get_file[from] != get_file[to]
                && get_rank[from] != get_rank[to]
                && get_diagonal[from] != get_diagonal[to]
                && get_antidiagonal(from) != get_antidiagonal(to))
                return false;

            if (RayBetween(from, to) & pos->Occupancy(BOTH))
                return false;

            break;

        case KING:
            if (isCastle(move)) {
                if (pos->boardState.checkers)
                    return false;

                if (std::abs(to - from) != 2)
                    return false;

                bool isKSCastle = GetMovetype(move) == static_cast<int>(Movetype::KSCastle);

                Bitboard castleBlocked = pos->Occupancy(BOTH) & (pos->side == WHITE ? isKSCastle ? 0x6000000000000000ULL
                                                                                                 : 0x0E00000000000000ULL
                                                                                    : isKSCastle ? 0x0000000000000060ULL
                                                                                                 : 0x000000000000000EULL);
                int castleType = pos->side == WHITE ? isKSCastle ? WKCA
                                                                 : WQCA
                                                    : isKSCastle ? BKCA
                                                                 : BQCA;

                if (!castleBlocked && (pos->GetCastlingPerm() & castleType))
                    return true;

                return false;
            }
            if (!(king_attacks[from] & (1ULL << to)))
                return false;

            break;
    }
    return true;
}

bool IsLegal(S_Board* pos, int move) {

    if (!IsPseudoLegal(pos, move))
        return false;

    int color = pos->side;
    int ksq = KingSQ(pos, color);
    const int from = From(move);
    const int to = To(move);
    const int movedPiece = Piece(move);
    const int pieceType = GetPieceType(movedPiece);

    if (isEnpassant(move)) {
        int offset = color == WHITE ? 8 : -8;
        int ourPawn = GetPiece(PAWN, color);
        int theirPawn = GetPiece(PAWN, color ^ 1);
        ClearPiece(ourPawn, from, pos);
        ClearPiece(theirPawn, to + offset, pos);
        AddPiece(ourPawn, to, pos);
        bool isLegal = !IsSquareAttacked(pos, ksq, color ^ 1);
        AddPiece(ourPawn, from, pos);
        AddPiece(theirPawn, to + offset, pos);
        ClearPiece(ourPawn, to, pos);
        return isLegal;
    }
    else if (isCastle(move)) {
        bool isKSCastle = GetMovetype(move) == static_cast<int>(Movetype::KSCastle);
        if (isKSCastle) {
            return    !IsSquareAttacked(pos, color == WHITE ? f1 : f8, color ^ 1)
                   && !IsSquareAttacked(pos, color == WHITE ? g1 : g8, color ^ 1);
        }
        else {
            return    !IsSquareAttacked(pos, color == WHITE ? d1 : d8, color ^ 1)
                   && !IsSquareAttacked(pos, color == WHITE ? c1 : c8, color ^ 1);
        }
    }

    Bitboard pins = pos->boardState.pinD | pos->boardState.pinHV;

    if (pieceType == KING) {
        int king = GetPiece(KING, color);
        ClearPiece(king, ksq, pos);
        bool isLegal = !IsSquareAttacked(pos, to, color ^ 1);
        AddPiece(king, ksq, pos);
        return isLegal;
    }
    else if (pins & (1ULL << from)) {
        return !pos->boardState.checkers && (((1ULL << to) & RayBetween(ksq, from)) || ((1ULL << from) & RayBetween(ksq, to)));
    }
    else if (pos->boardState.checkers) {
        return (1ULL << to) & pos->boardState.checkMask;
    }
    else
        return true;
}