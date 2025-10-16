#include "movegen.h"
#include "attack.h"
#include "bitboard.h"
#include "init.h"
#include "io.h"
#include "makemove.h"
#include "position.h"
#include "move.h"

// is the square given in input attacked by the current given side
bool IsSquareAttacked(const Position* pos, const int square, const int side) {
    // Take the occupancies of both positions, encoding where all the pieces on the board reside
    const Bitboard occ = pos->Occupancy(BOTH);
    // is the square attacked by pawns
    if (getPawnAttacks(square, side ^ 1) & pos->getPieceColorBB(PAWN, side))
        return true;
    // is the square attacked by knights
    if (getKnightAttacks(square) & pos->getPieceColorBB(KNIGHT, side))
        return true;
    // is the square attacked by kings
    if (getKingAttacks(square) & pos->getPieceColorBB(KING, side))
        return true;
    // is the square attacked by bishops
    if (getBishopAttacks(square, occ) & (pos->getPieceColorBB(BISHOP, side) | pos->getPieceColorBB(QUEEN, side)))
        return true;
    // is the square attacked by rooks
    if (getRookAttacks(square, occ) & (pos->getPieceColorBB(ROOK, side) | pos->getPieceColorBB(QUEEN, side)))
        return true;
    // by default return false
    return false;
}

// Check for move legality by generating the list of legal moves in a position and checking if that move is present
bool MoveExists(Position* pos, const Move move) {
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
void AddMove(const Move move, MoveList* list) {
    list->moves[list->count].move = move;
    list->count++;
}

// function that adds an (already-scored) move to a move list
void AddMove(const Move move, const int score, MoveList* list) {
    list->moves[list->count].move = move;
    list->moves[list->count].score = score;
    list->count++;
}

static inline Bitboard NORTH(const Bitboard in, const int color) {
    return color == WHITE ? in >> 8 : in << 8;
}

static inline void PseudoLegalPawnMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    const Bitboard enemy = pos->Occupancy(color ^ 1);
    const Bitboard ourPawns = pos->getPieceColorBB(PAWN, color);
    const Bitboard rank4BB = color == WHITE ? 0x000000FF00000000ULL : 0x00000000FF000000ULL;
    const Bitboard freeSquares = ~pos->Occupancy(BOTH);
    const int pawnType = GetPiece(PAWN, pos->side);
    const int north = color == WHITE ? -8 : 8;
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;

    // Quiet moves (ie push/double-push)
    if (genQuiet) {
        Bitboard push = NORTH(ourPawns, color) & freeSquares & ~0xFF000000000000FFULL;
        Bitboard doublePush = NORTH(push, color) & freeSquares & rank4BB;
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
        Bitboard pushPromo = NORTH(ourPawns, color) & freeSquares & 0xFF000000000000FFULL;
        while (pushPromo) {
            const int to = popLsb(pushPromo);
            AddMove(encode_move(to - north, to, pawnType, Movetype::queenPromo | Movetype::Quiet), list);
            AddMove(encode_move(to - north, to, pawnType, Movetype::rookPromo | Movetype::Quiet), list);
            AddMove(encode_move(to - north, to, pawnType, Movetype::bishopPromo | Movetype::Quiet), list);
            AddMove(encode_move(to - north, to, pawnType, Movetype::knightPromo | Movetype::Quiet), list);
        }

        // Captures and capture-promotions
        Bitboard captBB1 = (NORTH(ourPawns, color) >> 1) & ~0x8080808080808080ULL & enemy;
        Bitboard captBB2 = (NORTH(ourPawns, color) << 1) & ~0x101010101010101ULL & enemy;
        while (captBB1) {
            const int to = popLsb(captBB1);
            const int from = to - north + 1;
            if ((0xFF000000000000FFULL >> to) & 1) {
                AddMove(encode_move(from, to, pawnType, (Movetype::queenPromo | Movetype::Capture)), list);
                AddMove(encode_move(from, to, pawnType, (Movetype::rookPromo | Movetype::Capture)), list); 
                AddMove(encode_move(from, to, pawnType, (Movetype::bishopPromo | Movetype::Capture)), list);
                AddMove(encode_move(from, to, pawnType, (Movetype::knightPromo | Movetype::Capture)), list);
            }
            else AddMove(encode_move(from, to, pawnType, Movetype::Capture), list);
        }

        while (captBB2) {
            const int to = popLsb(captBB2);
            const int from = to - north - 1;
            if ((0xFF000000000000FFULL >> to) & 1) {
                AddMove(encode_move(from, to, pawnType, (Movetype::queenPromo | Movetype::Capture)), list);
                AddMove(encode_move(from, to, pawnType, (Movetype::rookPromo | Movetype::Capture)), list); 
                AddMove(encode_move(from, to, pawnType, (Movetype::bishopPromo | Movetype::Capture)), list);
                AddMove(encode_move(from, to, pawnType, (Movetype::knightPromo | Movetype::Capture)), list);
            }
            else AddMove(encode_move(from, to, pawnType, Movetype::Capture), list);
        }

        const int epSq = pos->getEpSquare();
        if (epSq == no_sq)
            return;

        // En passant
        Bitboard epPieces = getPawnAttacks(epSq, color ^ 1) & ourPawns;
        while (epPieces) {
            int from = popLsb(epPieces);
            AddMove(encode_move(from, epSq, pawnType, Movetype::enPassant), list);
        }
    }
}

static inline void PseudoLegalKnightMoves(Position* pos, int color, MoveList* list, MovegenType type) {
    Bitboard knights = pos->getPieceColorBB(KNIGHT, color);
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
        Bitboard possible_moves = getKnightAttacks(from) & moveMask;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            const Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
            AddMove(encode_move(from, to, knightType, movetype), list);
        }
    }
}

static inline void PseudoLegalSlidersMoves(Position *pos, int color, MoveList *list, MovegenType type) {
    const bool genNoisy = type & MOVEGEN_NOISY;
    const bool genQuiet = type & MOVEGEN_QUIET;
    Bitboard boardOccupancy = pos->Occupancy(BOTH);
    Bitboard moveMask = 0ULL; // We restrict the number of squares the bishop can travel to

    // The type requested includes noisy moves
    if (genNoisy)
        moveMask |= pos->Occupancy(color ^ 1);

    // The type requested includes quiet moves
    if (genQuiet)
        moveMask |= ~pos->Occupancy(BOTH);

    for (int piecetype = BISHOP; piecetype <= QUEEN; piecetype++) {
        Bitboard pieces = pos->getPieceColorBB(piecetype, color);
        const int coloredPieceValue = GetPiece(piecetype, color);
        while (pieces) {
            const int from = popLsb(pieces);
            Bitboard possible_moves =
                    pieceAttacks(piecetype, from, boardOccupancy) & moveMask;
            while (possible_moves) {
                const int to = popLsb(possible_moves);
                const Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
                AddMove(encode_move(from, to, coloredPieceValue, movetype), list);
            }
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

    Bitboard possible_moves = getKingAttacks(from) & moveMask;
    while (possible_moves) {
        const int to = popLsb(possible_moves);
        Movetype movetype = pos->PieceOn(to) != EMPTY ? Movetype::Capture : Movetype::Quiet;
        AddMove(encode_move(from, to, kingType, movetype), list);
    }

    // Only generate castling moves if we are generating quiets
    // Castling is illegal in check
    if (genQuiet && !pos->getCheckers()) {
        const Bitboard occ = pos->Occupancy(BOTH);
        const int castlePerms = pos->getCastlingPerm();
        if (color == WHITE) {
            // king side castling is available
            if ((castlePerms & WKCA) && !(occ & 0x6000000000000000ULL))
                AddMove(encode_move(e1, g1, WK, Movetype::KSCastle), list);

            // queen side castling is available
            if ((castlePerms & WQCA) && !(occ & 0x0E00000000000000ULL))
                AddMove(encode_move(e1, c1, WK, Movetype::QSCastle), list);
        }
        else {
            // king side castling is available
            if ((castlePerms & BKCA) && !(occ & 0x0000000000000060ULL))
                AddMove(encode_move(e8, g8, BK, Movetype::KSCastle), list);

            // queen side castling is available
            if ((castlePerms & BQCA) && !(occ & 0x000000000000000EULL))
                AddMove(encode_move(e8, c8, BK, Movetype::QSCastle), list);
        }
    }
}

// generate moves
void GenerateMoves(MoveList* move_list, Position* pos, MovegenType type) {

    assert(type == MOVEGEN_ALL || type == MOVEGEN_NOISY || type == MOVEGEN_QUIET);

    const int checks = CountBits(pos->getCheckers());
    if (checks < 2) {
        PseudoLegalPawnMoves(pos, pos->side, move_list, type);
        PseudoLegalKnightMoves(pos, pos->side, move_list, type);
        PseudoLegalSlidersMoves(pos, pos->side, move_list, type);
    }
    PseudoLegalKingMoves(pos, pos->side, move_list, type);
}

// generate moves
void generateQuietChecks(MoveList* movelist, Position* pos) {
    const int stm = pos->side;
    const int nstm = !stm;
    const Square oppKingSq = KingSQ(pos, nstm);
    const Bitboard occupied = pos->Occupancy(BOTH);
    const Bitboard pinned = pos->getPinnedMask(stm);

    // For each piece type, precompute the squares from which that piece could give a
    // check to the opponent's king if it moved there — and the square is empty.
    Bitboard pawnCheckSquares = getPawnAttacks(oppKingSq, nstm) & ~occupied;
    if (stm == WHITE) {
        // pick any white pawn that can give check
        Bitboard whitePawns = (pawnCheckSquares << 8) & pos->getPieceColorBB(PAWN, WHITE);
        // generate the moves for all the pawns we've got
        while (whitePawns) {
            const int from = popLsb(whitePawns);
            AddMove(encode_move(from, from - 8, WP, Movetype::Quiet), movelist);
        }
    }
    else {
        Bitboard blackPawns = (pawnCheckSquares >> 8) & pos->getPieceColorBB(PAWN, BLACK);
        while (blackPawns) {
            const int from = popLsb(blackPawns);
            AddMove(encode_move(from, from + 8, BP, Movetype::Quiet), movelist);
        }
    }

    Bitboard knights = pos->getPieceColorBB(KNIGHT, stm) & ~pinned;
    Bitboard knightCheckSquares = getKnightAttacks(oppKingSq) & ~occupied;
    const int knightType = GetPiece(KNIGHT, stm);
    while (knights) {
        const int from = popLsb(knights);
        Bitboard possible_moves = getKnightAttacks(from) & knightCheckSquares;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            AddMove(encode_move(from, to, knightType, Movetype::Quiet), movelist);
        }
    }

    Bitboard bishops = pos->getPieceColorBB(BISHOP, stm);
    Bitboard bishopCheckSquares = getBishopAttacks(oppKingSq, occupied) & ~occupied;
    const int bishopType = GetPiece(BISHOP, stm);
    while (bishops) {
        const int from = popLsb(bishops);
        Bitboard possible_moves = getBishopAttacks(from, occupied) & bishopCheckSquares;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            AddMove(encode_move(from, to, bishopType, Movetype::Quiet), movelist);
        }
    }

    Bitboard rooks = pos->getPieceColorBB(ROOK, stm);
    Bitboard rookCheckSquares = getRookAttacks(oppKingSq, occupied) & ~occupied;
    const int rookType = GetPiece(ROOK, stm);
    while (rooks) {
        const int from = popLsb(rooks);
        Bitboard possible_moves = getRookAttacks(from, occupied) & rookCheckSquares;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            AddMove(encode_move(from, to, rookType, Movetype::Quiet), movelist);
        }
    }

    Bitboard queens = pos->getPieceColorBB(QUEEN, stm);
    Bitboard queenCheckSquares = bishopCheckSquares | rookCheckSquares;
    const int queenType = GetPiece(QUEEN, stm);
    while (queens) {
        const int from = popLsb(queens);
        Bitboard possible_moves = getQueenAttacks(from, occupied) & queenCheckSquares;
        while (possible_moves) {
            const int to = popLsb(possible_moves);
            AddMove(encode_move(from, to, queenType, Movetype::Quiet), movelist);
        }
    }
}

// Pseudo-legality test inspired by Koivisto
bool IsPseudoLegal(Position* pos, Move move) {

    if (move == NOMOVE)
        return false;

    const Square from = From(move);
    const Square to = To(move);
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

    if (pieceType != KING && CountBits(pos->getCheckers()) >= 2)
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
                if (to != pos->getEpSquare())
                    return false;

                if (!((1ULL << (to - NORTH)) & pos->getPieceColorBB(PAWN, pos->side ^ 1)))
                    return false;
            }
            if (isCapture(move) && !(getPawnAttacks(from, pos->side) & (1ULL << to)))
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
            if (!(getKnightAttacks(from) & (1ULL << to)))
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
                if (pos->getCheckers())
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

                if (!castleBlocked && (pos->getCastlingPerm() & castleType))
                    return true;

                return false;
            }
            if (!(getKingAttacks(from) & (1ULL << to)))
                return false;

            break;
    }
    return true;
}

bool IsLegal(Position* pos, Move move) {

    const int color = pos->side;
    const Square ksq = KingSQ(pos, color);
    const Square from = From(move);
    const Square to = To(move);
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
    else if (pos->getPinnedMask(pos->side) & (1ULL << from)) {
        return !pos->getCheckers() && (((1ULL << to) & RayBetween(ksq, from)) || ((1ULL << from) & RayBetween(ksq, to)));
    }
    else if (pos->getCheckers()) {
        return (1ULL << to) & (pos->getCheckers() | RayBetween(GetLsbIndex(pos->getCheckers()), ksq));
    }
    else
        return true;
}
