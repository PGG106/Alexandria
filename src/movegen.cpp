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

static inline Bitboard NORTH(Bitboard in, int color) {
    return color == WHITE ? in >> 8 : in << 8;
}

static inline void PseudoLegalPawnMoves(S_Board* pos, int color, S_MOVELIST* list, bool capOnly) {

    const Bitboard enemy = pos->Occupancy(color ^ 1);
    const Bitboard ourPawns = pos->GetPieceColorBB(PAWN, color);
    const Bitboard rank4BB = color == WHITE ? 0x000000FF00000000ULL : 0x00000000FF000000ULL;
    const int pawnType = GetPiece(PAWN, pos->side);
    const int north = color == WHITE ? -8 : 8;

    // Quiet moves (ie push/double-push)
    if (!capOnly) {
        Bitboard push = NORTH(ourPawns, color) & ~pos->Occupancy(BOTH) & ~0xFF000000000000FFULL;
        Bitboard doublePush = NORTH(push, color) & ~pos->Occupancy(BOTH) & rank4BB;
        push &= pos->checkMask;
        doublePush &= pos->checkMask;
        while (push) {
            int to = popLsb(push);
            AddMove(encode_move(to - north, to, pawnType, Movetype::Quiet), list);
        }
        while (doublePush) {
            int to = popLsb(doublePush);
            AddMove(encode_move(to - north * 2, to, pawnType, Movetype::doublePush), list);
        }
    }

    // Push promotions
    Bitboard pushPromo = NORTH(ourPawns, color) & ~pos->Occupancy(BOTH) & 0xFF000000000000FFULL & pos->checkMask;
    while (pushPromo) {
        int to = popLsb(pushPromo);
        AddMove(encode_move(to - north, to, pawnType, Movetype::queenPromo | Movetype::Quiet), list);
        AddMove(encode_move(to - north, to, pawnType, Movetype::rookPromo | Movetype::Quiet), list);
        AddMove(encode_move(to - north, to, pawnType, Movetype::bishopPromo | Movetype::Quiet), list);
        AddMove(encode_move(to - north, to, pawnType, Movetype::knightPromo | Movetype::Quiet), list);
    }

    // Captures and capture-promotions
    Bitboard captBB1 = (NORTH(ourPawns & ~0x8080808080808080ULL, color) >> 1) & enemy & pos->checkMask;
    Bitboard captBB2 = (NORTH(ourPawns & ~0x101010101010101ULL, color) << 1) & enemy & pos->checkMask;
    while (captBB1) {
        int to = popLsb(captBB1);
        int from = to - north + 1;
        if (get_rank[to] != (color == WHITE ? 7 : 0))
            AddMove(encode_move(from, to, pawnType, Movetype::Capture), list);
        else {
            AddMove(encode_move(from, to, pawnType, (Movetype::queenPromo | Movetype::Capture)), list);
            AddMove(encode_move(from, to, pawnType, (Movetype::rookPromo | Movetype::Capture)), list); 
            AddMove(encode_move(from, to, pawnType, (Movetype::bishopPromo | Movetype::Capture)), list);
            AddMove(encode_move(from, to, pawnType, (Movetype::knightPromo | Movetype::Capture)), list);
        }
    }

    while (captBB2) {
        int to = popLsb(captBB2);
        int from = to - north - 1;
        if (get_rank[to] != (color == WHITE ? 7 : 0))
            AddMove(encode_move(from, to, pawnType, Movetype::Capture), list);
        else {
            AddMove(encode_move(from, to, pawnType, (Movetype::queenPromo | Movetype::Capture)), list);
            AddMove(encode_move(from, to, pawnType, (Movetype::rookPromo | Movetype::Capture)), list); 
            AddMove(encode_move(from, to, pawnType, (Movetype::bishopPromo | Movetype::Capture)), list);
            AddMove(encode_move(from, to, pawnType, (Movetype::knightPromo | Movetype::Capture)), list);
        }
    }

    int epSq = GetEpSquare(pos);
    if (epSq == no_sq)
        return;

    // En passant
    Bitboard epPieces = pawn_attacks[color ^ 1][epSq] & ourPawns;
    while (epPieces) {
        int from = popLsb(epPieces);
        AddMove(encode_move(from, epSq, pawnType, Movetype::enPassant), list);
    }
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
        PseudoLegalPawnMoves(pos, pos->side, move_list, false);

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
        PseudoLegalPawnMoves(pos, pos->side, move_list, true);

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