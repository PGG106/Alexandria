#include "movegen.h"
#include "attack.h"
#include "bitboard.h"
#include "init.h"
#include "magic.h"
#include "makemove.h"
#include "position.h"
#include "move.h"

// is the square given in input attacked by the current given side
bool IsSquareAttacked(const Position* pos, const int square, const int side) {
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
bool MoveExists(Position* pos, const int move) {
    MoveList list;
    GenerateMoves(&list, pos, MOVEGEN_ALL);

    for (int moveNum = 0; moveNum < list.count; ++moveNum) {
        if (list.moves[moveNum].move == move) {
            return IsLegal(pos, move);
        }
    }
    return false;
}

// function that adds a (not yet scored) move to a move list
void AddMove(const int move, MoveList* list) {
    list->moves[list->count].move = move;
    list->count++;
}

// function that adds an (already-scored) move to a move list
void AddMove(const int move, const int score, MoveList* list) {
    list->moves[list->count].move = move;
    list->moves[list->count].score = score;
    list->count++;
}

static inline Bitboard NORTH(const Bitboard in, const int color) {
    return color == WHITE ? in >> 8 : in << 8;
}

static inline void PseudoLegalPawnMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    const Bitboard enemy = pos->Occupancy(color ^ 1);
    const Bitboard ourPawns = pos->GetPieceColorBB(PAWN, color);
    const Bitboard rank4BB = color == WHITE ? 0x000000FF00000000ULL : 0x00000000FF000000ULL;
    const int pawnType = GetPiece(PAWN, pos->side);
    const int north = color == WHITE ? -8 : 8;
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;

    // Quiet moves (ie push/double-push)
    if (genQuiet) {
        Bitboard push = NORTH(ourPawns, color) & ~pos->Occupancy(BOTH) & ~0xFF000000000000FFULL;
        Bitboard doublePush = NORTH(push, color) & ~pos->Occupancy(BOTH) & rank4BB;
        push &= pos->checkMask;
        doublePush &= pos->checkMask;
        while (push) {
            const int to = popLsb(push);
            AddMove(encode_move(to - north, to, pawnType, Movetype::Quiet), list);
        }
        while (doublePush) {
            const int to = popLsb(doublePush);
            AddMove(encode_move(to - north * 2, to, pawnType, Movetype::doublePush), list);
        }
    }

    if (genNoisy) {

        // Push promotions
        Bitboard pushPromo = NORTH(ourPawns, color) & ~pos->Occupancy(BOTH) & 0xFF000000000000FFULL & pos->checkMask;
        while (pushPromo) {
            const int to = popLsb(pushPromo);
            AddMove(encode_move(to - north, to, pawnType, Movetype::queenPromo | Movetype::Quiet), list);
            AddMove(encode_move(to - north, to, pawnType, Movetype::rookPromo | Movetype::Quiet), list);
            AddMove(encode_move(to - north, to, pawnType, Movetype::bishopPromo | Movetype::Quiet), list);
            AddMove(encode_move(to - north, to, pawnType, Movetype::knightPromo | Movetype::Quiet), list);
        }

        // Captures and capture-promotions
        Bitboard captBB1 = (NORTH(ourPawns, color) >> 1) & ~0x8080808080808080ULL & enemy & pos->checkMask;
        Bitboard captBB2 = (NORTH(ourPawns, color) << 1) & ~0x101010101010101ULL & enemy & pos->checkMask;
        while (captBB1) {
            const int to = popLsb(captBB1);
            const int from = to - north + 1;
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
            const int to = popLsb(captBB2);
            const int from = to - north - 1;
            if (get_rank[to] != (color == WHITE ? 7 : 0))
                AddMove(encode_move(from, to, pawnType, Movetype::Capture), list);
            else {
                AddMove(encode_move(from, to, pawnType, (Movetype::queenPromo | Movetype::Capture)), list);
                AddMove(encode_move(from, to, pawnType, (Movetype::rookPromo | Movetype::Capture)), list); 
                AddMove(encode_move(from, to, pawnType, (Movetype::bishopPromo | Movetype::Capture)), list);
                AddMove(encode_move(from, to, pawnType, (Movetype::knightPromo | Movetype::Capture)), list);
            }
        }

        const int epSq = GetEpSquare(pos);
        if (epSq == no_sq)
            return;

        // En passant
        Bitboard epPieces = pawn_attacks[color ^ 1][epSq] & ourPawns;
        while (epPieces) {
            int from = popLsb(epPieces);
            AddMove(encode_move(from, epSq, pawnType, Movetype::enPassant), list);
        }
    }
}

static inline void PseudoLegalKnightMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    Bitboard knights = pos->GetPieceColorBB(KNIGHT, color);
    const int knightType = GetPiece(KNIGHT, color);
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;
    Bitboard moveMask = 0ULL; // We restrict the number of squares the knight can travel to

    // The type requested includes noisy moves
    if (genNoisy)
        moveMask |= pos->Occupancy(color ^ 1);

    // The type requested includes quiet moves
    if (genQuiet)
        moveMask |= ~pos->Occupancy(BOTH);

    while (knights) {
        const int from = popLsb(knights);
        Bitboard possible_moves = knight_attacks[from] & moveMask & pos->checkMask;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
            AddMove(encode_move(from, to, knightType, movetype), list);
        }
    }
}

static inline void PseudoLegalBishopMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    Bitboard bishops = pos->GetPieceColorBB(BISHOP, color);
    const int bishopType = GetPiece(BISHOP, color);
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;
    Bitboard moveMask = 0ULL; // We restrict the number of squares the bishop can travel to

    // The type requested includes noisy moves
    if (genNoisy)
        moveMask |= pos->Occupancy(color ^ 1);

    // The type requested includes quiet moves
    if (genQuiet)
        moveMask |= ~pos->Occupancy(BOTH);

    while (bishops) {
        const int from = popLsb(bishops);
        Bitboard possible_moves = GetBishopAttacks(from, pos->Occupancy(BOTH)) & moveMask & pos->checkMask;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
            AddMove(encode_move(from, to, bishopType, movetype), list);
        }
    }
}

static inline void PseudoLegalRookMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    Bitboard rooks = pos->GetPieceColorBB(ROOK, color);
    const int rookType = GetPiece(ROOK, color);
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;
    Bitboard moveMask = 0ULL; // We restrict the number of squares the rook can travel to

    // The type requested includes noisy moves
    if (genNoisy)
        moveMask |= pos->Occupancy(color ^ 1);

    // The type requested includes quiet moves
    if (genQuiet)
        moveMask |= ~pos->Occupancy(BOTH);

    while (rooks) {
        const int from = popLsb(rooks);
        Bitboard possible_moves = GetRookAttacks(from, pos->Occupancy(BOTH)) & moveMask & pos->checkMask;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
            AddMove(encode_move(from, to, rookType, movetype), list);
        }
    }
}

static inline void PseudoLegalQueenMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    Bitboard queens = pos->GetPieceColorBB(QUEEN, color);
    const int queenType = GetPiece(QUEEN, color);
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;
    Bitboard moveMask = 0ULL; // We restrict the number of squares the queen can travel to

    // The type requested includes noisy moves
    if (genNoisy)
        moveMask |= pos->Occupancy(color ^ 1);

    // The type requested includes quiet moves
    if (genQuiet)
        moveMask |= ~pos->Occupancy(BOTH);

    while (queens) {
        const int from = popLsb(queens);
        Bitboard possible_moves = GetQueenAttacks(from, pos->Occupancy(BOTH)) & moveMask & pos->checkMask;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
            AddMove(encode_move(from, to, queenType, movetype), list);
        }
    }
}

static inline void PseudoLegalKingMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    const int kingType = GetPiece(KING, color);
    const int from = KingSQ(pos, color);
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;
    Bitboard moveMask = 0ULL; // We restrict the number of squares the king can travel to

    // The type requested includes noisy moves
    if (genNoisy)
        moveMask |= pos->Occupancy(color ^ 1);

    // The type requested includes quiet moves
    if (genQuiet)
        moveMask |= ~pos->Occupancy(BOTH);

    Bitboard possible_moves = king_attacks[from] & moveMask;
    while (possible_moves) {
        const int to = popLsb(possible_moves);
        Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
        AddMove(encode_move(from, to, kingType, movetype), list);
    }

    // Only generate castling moves if we are generating quiets
    // Castling is illegal in check
    if (genQuiet && !pos->checkers) {
        if (color == WHITE) {
            // king side castling is available
            if ((pos->GetCastlingPerm() & WKCA) && !(pos->Occupancy(BOTH) & 0x6000000000000000ULL))
                AddMove(encode_move(e1, g1, WK, Movetype::KSCastle), list);

            // queen side castling is available
            if ((pos->GetCastlingPerm() & WQCA) && !(pos->Occupancy(BOTH) & 0x0E00000000000000ULL))
                AddMove(encode_move(e1, c1, WK, Movetype::QSCastle), list);
        }

        else {
            // king side castling is available
            if ((pos->GetCastlingPerm() & BKCA) && !(pos->Occupancy(BOTH) & 0x0000000000000060ULL))
                AddMove(encode_move(e8, g8, BK, Movetype::KSCastle), list);

            // queen side castling is available
            if ((pos->GetCastlingPerm() & BQCA) && !(pos->Occupancy(BOTH) & 0x000000000000000EULL))
                AddMove(encode_move(e8, c8, BK, Movetype::QSCastle), list);
        }
    }
}

// generate moves
void GenerateMoves(MoveList* move_list, Position* pos, MovegenType type) {

    assert(type == MOVEGEN_ALL || type == MOVEGEN_NOISY || type == MOVEGEN_QUIET);

    const int checks = CountBits(pos->checkers);
    if (checks < 2) {
        PseudoLegalPawnMoves(pos, pos->side, move_list, type);
        PseudoLegalKnightMoves(pos, pos->side, move_list, type);
        PseudoLegalBishopMoves(pos, pos->side, move_list, type);
        PseudoLegalRookMoves(pos, pos->side, move_list, type);
        PseudoLegalQueenMoves(pos, pos->side, move_list, type);
    }
    PseudoLegalKingMoves(pos, pos->side, move_list, type);
}

// Pseudo-legality test inspired by Koivisto
bool IsPseudoLegal(Position* pos, int move) {

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

    const int NORTH = pos->side == WHITE ? -8 : 8;

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

bool IsLegal(Position* pos, int move) {

    const int color = pos->side;
    const int ksq = KingSQ(pos, color);
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

    if (pieceType == KING) {
        int king = GetPiece(KING, color);
        ClearPiece(king, ksq, pos);
        bool isLegal = !IsSquareAttacked(pos, to, color ^ 1);
        AddPiece(king, ksq, pos);
        return isLegal;
    }
    else if (pos->pinned & (1ULL << from)) {
        return !pos->checkers && (((1ULL << to) & RayBetween(ksq, from)) || ((1ULL << from) & RayBetween(ksq, to)));
    }
    else if (pos->checkers) {
        return (1ULL << to) & pos->checkMask;
    }
    else
        return true;
}