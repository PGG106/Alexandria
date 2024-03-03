#include "movegen.h"
#include "attack.h"
#include "bitboard.h"
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
    return 1ULL << (sq + (color == WHITE ? -8 : 8));
}

// Check for move legality by generating the list of legal moves in a position and checking if that move is present
bool MoveExists(S_Board* pos, const int move) {
    S_MOVELIST list[1];
    GenerateMoves(list, pos);

    for (int moveNum = 0; moveNum < list->count; ++moveNum) {
        if (list->moves[moveNum].move == move) {
            return IsLegal(pos, move);
        }
    }
    return false;
}
// function that adds a move to the move list
void AddMove(int move, S_MOVELIST* list) {
    list->moves[list->count].move = move;
    list->moves[list->count].score = 0;
    list->count++;
}
// function that adds a pawn move (and all its possible branches) to the move list
static inline void AddPawnMove(const S_Board* pos, const int from, const int to, S_MOVELIST* list) {
    Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
    if (abs(to - from) == 16)
        movetype = Movetype::doublePush;
    else if (to == GetEpSquare(pos))
        movetype = Movetype::enPassant;

    int pawnType = GetPiece(PAWN, pos->side);

    // if the pawn is moving from the 7th to the 8th rank (relative), it is a promotion
    if (get_rank[to] == (pos->side == WHITE ? 7 : 0)) {
        // consider every possible piece promotion
        AddMove(encode_move(from, to, pawnType, (Movetype::queenPromo | movetype)), list);
        AddMove(encode_move(from, to, pawnType, (Movetype::rookPromo | movetype)), list); 
        AddMove(encode_move(from, to, pawnType, (Movetype::bishopPromo | movetype)), list);
        AddMove(encode_move(from, to, pawnType, (Movetype::knightPromo | movetype)), list);
    }
    // not a promotion
    else
        AddMove(encode_move(from, to, pawnType,  movetype), list);
}

static inline Bitboard PseudoLegalPawnMoves(S_Board* pos, int color, int square) {

    const Bitboard enemy = pos->Enemy();
    Bitboard moves = 0;

    // Calculate pawn pushs
    Bitboard push = PawnPush(color, square) & ~pos->Occupancy(BOTH);

    push |= color == WHITE ? (get_rank[square] == 1 ? (push >> 8) & ~pos->Occupancy(BOTH) : 0ULL)
                           : (get_rank[square] == 6 ? (push << 8) & ~pos->Occupancy(BOTH) : 0ULL);

    moves |= push;

    int epSq = GetEpSquare(pos);
    Bitboard attacks = pawn_attacks[color][square];
    moves |= attacks & enemy;
    if (epSq != no_sq && (attacks & (1ULL << epSq)))
        moves |= 1ULL << epSq;

    moves &= pos->checkMask;

    return moves;
}

static inline Bitboard PseudoLegalKnightMoves(S_Board* pos, int color, int square) {
    return knight_attacks[square] & ~pos->Occupancy(color) & pos->checkMask;
}

static inline Bitboard PseudoLegalBishopMoves(S_Board* pos, int color, int square) {
    return GetBishopAttacks(square, pos->Occupancy(BOTH)) & ~pos->Occupancy(color) & pos->checkMask;
}

static inline Bitboard PseudoLegalRookMoves(S_Board* pos, int color, int square) {
    return GetRookAttacks(square, pos->Occupancy(BOTH)) & ~pos->Occupancy(color) & pos->checkMask;
}

static inline Bitboard PseudoLegalQueenMoves(S_Board* pos, int color, int square) {
    return PseudoLegalRookMoves(pos, color, square) | PseudoLegalBishopMoves(pos, color, square);
}

static inline Bitboard PseudoLegalKingMoves(S_Board* pos, int color, int square) {
    return king_attacks[square] & ~pos->Occupancy(color);
}

// generate all moves
void GenerateMoves(S_MOVELIST* move_list, S_Board* pos) {
    // init move count
    move_list->count = 0;

    // define source & target squares
    int sourceSquare, targetSquare;

    const int checks = CountBits(pos->checkers);
    
    if (checks < 2) {
        Bitboard pawns = pos->GetPieceColorBB(PAWN, pos->side);
        while (pawns) {
            // init source square
            sourceSquare = popLsb(pawns);
            Bitboard moves = PseudoLegalPawnMoves(pos, pos->side, sourceSquare);
            while (moves) {
                // init target square
                targetSquare = popLsb(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
            }
        }

        // genarate knight moves
        Bitboard knights = pos->GetPieceColorBB(KNIGHT, pos->side);
        while (knights) {
            sourceSquare = popLsb(knights);
            Bitboard moves = PseudoLegalKnightMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(KNIGHT, pos->side);
            while (moves) {
                targetSquare = popLsb(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
            }
        }

        Bitboard bishops = pos->GetPieceColorBB(BISHOP, pos->side);
        while (bishops) {
            sourceSquare = popLsb(bishops);
            Bitboard moves = PseudoLegalBishopMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(BISHOP, pos->side);
            while (moves) {
                targetSquare = popLsb(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
            }
        }

        Bitboard rooks = pos->GetPieceColorBB(ROOK, pos->side);
        while (rooks) {
            sourceSquare = popLsb(rooks);
            Bitboard moves = PseudoLegalRookMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(ROOK, pos->side);
            while (moves) {
                targetSquare = popLsb(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
            }
        }

        Bitboard queens = pos->GetPieceColorBB(QUEEN, pos->side);
        while (queens) {
            sourceSquare = popLsb(queens);
            Bitboard moves = PseudoLegalQueenMoves(pos, pos->side, sourceSquare);
            const int piece = GetPiece(QUEEN, pos->side);
            while (moves) {
                targetSquare = popLsb(moves);
                Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
            }
        }
    }

    sourceSquare = KingSQ(pos, pos->side);
    const int piece = GetPiece(KING, pos->side);
    Bitboard moves = PseudoLegalKingMoves(pos, pos->side, sourceSquare);
    while (moves) {
        targetSquare = popLsb(moves);
        Movetype movetype = pos->PieceOn(targetSquare) != EMPTY ? Movetype::Capture : Movetype::Quiet;
        AddMove(encode_move(sourceSquare, targetSquare, piece, movetype), move_list);
    }

    if (!pos->checkers) {
        if (pos->side == WHITE) {
            // king side castling is available
            if ((pos->GetCastlingPerm() & WKCA) && !(pos->Occupancy(BOTH) & 0x6000000000000000ULL))
                AddMove(encode_move(e1, g1, WK, Movetype::KSCastle), move_list);

            if ((pos->GetCastlingPerm() & WQCA) && !(pos->Occupancy(BOTH) & 0x0E00000000000000ULL))
                AddMove(encode_move(e1, c1, WK, Movetype::QSCastle), move_list);
        }

        else {
            if ((pos->GetCastlingPerm() & BKCA) && !(pos->Occupancy(BOTH) & 0x0000000000000060ULL))
                AddMove(encode_move(e8, g8, BK, Movetype::KSCastle), move_list);

            if ((pos->GetCastlingPerm() & BQCA) && !(pos->Occupancy(BOTH) & 0x000000000000000EULL))
                AddMove(encode_move(e8, c8, BK, Movetype::QSCastle), move_list);
        }
    }
}

// generate all captures
void GenerateCaptures(S_MOVELIST* move_list, S_Board* pos) {
    // init move count
    move_list->count = 0;

    // define source & target squares
    int sourceSquare, targetSquare;

    const int checks = CountBits(pos->checkers);

    if (checks < 2) {

        Bitboard pawn_mask = pos->GetPieceColorBB(PAWN, pos->side);

        while (pawn_mask) {
            // init source square
            sourceSquare = popLsb(pawn_mask);
            Bitboard moves = PseudoLegalPawnMoves(pos, pos->side, sourceSquare) & (pos->Enemy() | 0xFF000000000000FF | (1ULL << GetEpSquare(pos)));
            while (moves) {
                // init target square
                targetSquare = popLsb(moves);
                AddPawnMove(pos, sourceSquare, targetSquare, move_list);
            }
        }
        // genarate knight moves
        Bitboard knights_mask = pos->GetPieceColorBB(KNIGHT, pos->side);
        while (knights_mask) {
            sourceSquare = popLsb(knights_mask);
            Bitboard moves = PseudoLegalKnightMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(KNIGHT, pos->side);
            // while we have moves that the knight can play we add them to the list
            while (moves) {
                targetSquare = popLsb(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
            }
        }
        Bitboard bishops_mask = pos->GetPieceColorBB(BISHOP, pos->side);
        while (bishops_mask) {
            sourceSquare = popLsb(bishops_mask);
            Bitboard moves = PseudoLegalBishopMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(BISHOP, pos->side);
            while (moves) {
                targetSquare = popLsb(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
            }
        }
        Bitboard rooks_mask = pos->GetPieceColorBB(ROOK, pos->side);
        while (rooks_mask) {
            sourceSquare = popLsb(rooks_mask);
            Bitboard moves = PseudoLegalRookMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(ROOK, pos->side);
            while (moves) {
                targetSquare = popLsb(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
            }
        }
        Bitboard queens_mask = pos->GetPieceColorBB(QUEEN, pos->side);
        while (queens_mask) {
            sourceSquare = popLsb(queens_mask);
            Bitboard moves = PseudoLegalQueenMoves(pos, pos->side, sourceSquare) & pos->Enemy();
            const int piece = GetPiece(QUEEN, pos->side);
            while (moves) {
                targetSquare = popLsb(moves);
                AddMove(encode_move(sourceSquare, targetSquare, piece, Movetype::Capture), move_list);
            }
        }
    }

    sourceSquare = KingSQ(pos, pos->side);
    const int piece = GetPiece(KING, pos->side);
    Bitboard king_moves = PseudoLegalKingMoves(pos, pos->side, sourceSquare) & pos->Enemy();

    while (king_moves) {
        targetSquare = popLsb(king_moves);
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

    if ((CountBits(pos->checkers) >= 2) && pieceType != KING)
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
                if (pos->checkers)
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

    Bitboard pins = pos->pinD | pos->pinHV;

    if (pieceType == KING) {
        int king = GetPiece(KING, color);
        ClearPiece(king, ksq, pos);
        bool isLegal = !IsSquareAttacked(pos, to, color ^ 1);
        AddPiece(king, ksq, pos);
        return isLegal;
    }
    else if (pins & (1ULL << from)) {
        return !pos->checkers && (((1ULL << to) & RayBetween(ksq, from)) || ((1ULL << from) & RayBetween(ksq, to)));
    }
    else if (pos->checkers) {
        return (1ULL << to) & pos->checkMask;
    }
    else
        return true;
}