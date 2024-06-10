#include <cassert>
#include "bitboard.h"
#include "position.h"
#include "piece_data.h"
#include "makemove.h"
#include "misc.h"
#include "uci.h"
#include "attack.h"
#include "magic.h"
#include "init.h"
#include "cuckoo.h"

NNUE nnue = NNUE();

// Reset the position to a clean state
void ResetBoard(Position* pos) {
    // reset board position (pos->pos->bitboards)
    std::memset(pos->bitboards, 0ULL, sizeof(pos->bitboards));

    // reset pos->occupancies (pos->pos->bitboards)
    std::memset(pos->occupancies, 0ULL, sizeof(pos->occupancies));

    for (int index = 0; index < 64; ++index) {
        pos->pieces[index] = EMPTY;
    }
    pos->castleperm = 0; 
    pos->plyFromNull = 0;
}

void ResetInfo(SearchInfo* info) {
    info->depth = 0;
    info->nodes = 0;
    info->starttime = 0;
    info->stoptimeOpt = 0;
    info->stoptimeMax = 0;
    info->movestogo = -1;
    info->stopped = false;
    info->timeset = false;
    info->movetimeset = false;
    info->nodeset = false;
}

// Generates zobrist key from scratch
ZobristKey GeneratePosKey(const Position* pos) {
    Bitboard finalkey = 0;
    // for every square
    for (int sq = 0; sq < 64; ++sq) {
        // get piece on that square
        const int piece = pos->PieceOn(sq);
        // if it's not empty add that piece to the zobrist key
        if (piece != EMPTY) {
            assert(piece >= WP && piece <= BK);
            finalkey ^= PieceKeys[piece][sq];
        }
    }
    // include side in the key
    if (pos->side == BLACK) {
        finalkey ^= SideKey;
    }
    // include the ep square in the key
    if (GetEpSquare(pos) != no_sq) {
        assert(pos->enPas >= 0 && pos->enPas < 64);
        finalkey ^= enpassant_keys[GetEpSquare(pos)];
    }
    assert(pos->castleperm >= 0 && pos->castleperm <= 15);
    // add to the key the status of the castling permissions
    finalkey ^= CastleKeys[pos->GetCastlingPerm()];
    return finalkey;
}

// Generates zobrist key (for only the pawns) from scratch
ZobristKey GeneratePawnKey(const Position* pos) {
    Bitboard pawnKey = 0;
    for (int sq = 0; sq < 64; ++sq) {
        // get piece on that square
        const int piece = pos->PieceOn(sq);
        if (piece == WP || piece == BP) {
            pawnKey ^= PieceKeys[piece][sq];
        }
    }
    return pawnKey;
}

// parse FEN string
void ParseFen(const std::string& command, Position* pos) {

    ResetBoard(pos);

    std::vector<std::string> tokens = split_command(command);

    const std::string pos_string = tokens.at(0);
    const std::string turn = tokens.at(1);
    const std::string castle_perm = tokens.at(2);
    const std::string ep_square = tokens.at(3);
    std::string fifty_move;
    std::string HisPly;
    // Keep fifty move and Hisply arguments optional
    if (tokens.size() >= 5) {
        fifty_move = tokens.at(4);
        if (tokens.size() >= 6) {
            HisPly = tokens.at(5);
        }
    }

    int fen_counter = 0;
    for (int rank = 0; rank < 8; rank++) {
        // loop over board files
        for (int file = 0; file < 8; file++) {
            // init current square
            const int square = rank * 8 + file;
            const char current_char = pos_string[fen_counter];

            // match ascii pieces within FEN string
            if ((current_char >= 'a' && current_char <= 'z') || (current_char >= 'A' && current_char <= 'Z')) {
                // init piece type
                const int piece = char_pieces[current_char];
                if (piece != EMPTY) {
                    // set piece on corresponding bitboard
                    set_bit(pos->bitboards[piece], square);
                    pos->pieces[square] = piece;
                }
                fen_counter++;
            }

            // match empty square numbers within FEN string
            if (current_char >= '0' && current_char <= '9') {
                // init offset (convert char 0 to int 0)
                const int offset = current_char - '0';

                const int piece = pos->PieceOn(square);
                // on empty current square
                if (piece == EMPTY)
                    // decrement file
                    file--;

                // adjust file counter
                file += offset;

                // increment pointer to FEN string
                fen_counter++;
            }

            // match rank separator
            if (pos_string[fen_counter] == '/')
                // increment pointer to FEN string
                fen_counter++;
        }
    }

    // parse player turn
    pos->side = turn == "w" ? WHITE : BLACK;

    // Parse castling rights
    for (const char c : castle_perm) {
        switch (c) {
        case 'K':
            (pos->castleperm) |= WKCA;
            break;
        case 'Q':
            (pos->castleperm) |= WQCA;
            break;
        case 'k':
            (pos->castleperm) |= BKCA;
            break;
        case 'q':
            (pos->castleperm) |= BQCA;
            break;
        case '-':
            break;
        }
    }

    // parse enpassant square
    if (ep_square != "-" && ep_square.size() == 2) {
        // parse enpassant file & rank
        const int file = ep_square[0] - 'a';
        const int rank = 8 - (ep_square[1] - '0');

        // init enpassant square
        pos->enPas = rank * 8 + file;
    }
    // no enpassant square
    else
        pos->enPas = no_sq;

    // Read fifty moves counter
    if (!fifty_move.empty()) {
        pos->fiftyMove = std::stoi(fifty_move);
    }
    else {
        pos->fiftyMove = 0;
    }
    // Read Hisply moves counter
    if (!HisPly.empty()) {
        pos->hisPly = std::stoi(HisPly);
    }
    else {
        pos->hisPly = 0;
    }

    for (int piece = WP; piece <= WK; piece++)
        // populate white occupancy bitboard
        pos->occupancies[WHITE] |= pos->bitboards[piece];

    for (int piece = BP; piece <= BK; piece++)
        // populate white occupancy bitboard
        pos->occupancies[BLACK] |= pos->bitboards[piece];

    pos->posKey = GeneratePosKey(pos);
    pos->pawnKey = GeneratePawnKey(pos);

    // Update pinmasks and checkers
    UpdatePinsAndCheckers(pos, pos->side);

    // If we are in check get the squares between the checking piece and the king
    if (pos->checkers) {
        const int kingSquare = KingSQ(pos, pos->side);
        const int pieceLocation = GetLsbIndex(pos->checkers);
        pos->checkMask = (1ULL << pieceLocation) | RayBetween(pieceLocation, kingSquare);
    }
    else
        pos->checkMask = fullCheckmask;

    // Update nnue accumulator to reflect board state
    nnue.accumulate(pos->accumStack[0], pos);
    pos->accumStackHead = 1;
}

std::string GetFen(const Position* pos) {
    std::string pos_string;
    std::string turn;
    std::string castle_perm;
    std::string ep_square;
    std::string fifty_move;
    std::string HisPly;

    int empty_squares = 0;
    // Parse pieces
    for (int rank = 0; rank < 8; rank++) {
        // loop over board files
        for (int file = 0; file < 8; file++) {
            // init current square
            const int square = rank * 8 + file;
            const int potential_piece = pos->PieceOn(square);
            // If the piece isn't empty we add the empty squares counter to the fen string and then the piece
            if (potential_piece != EMPTY) {
                if (empty_squares) {
                    pos_string += std::to_string(empty_squares);
                    empty_squares = 0;
                }
                pos_string += ascii_pieces[potential_piece];
            } else {
                empty_squares++;
            }
        }
        // At the end of a file we dump the empty squares
        if (empty_squares) {
            pos_string += std::to_string(empty_squares);
            empty_squares = 0;
        }
        // skip the last file
        if (rank != 7)
            pos_string += '/';
    }
    // parse player turn
    turn = pos->side == WHITE ? "w" : "b";

    // Parse over castling rights
    if (pos->GetCastlingPerm() == 0)
        castle_perm = '-';
    else {
        if (pos->GetCastlingPerm() & WKCA)
            castle_perm += "K";
        if (pos->GetCastlingPerm() & WQCA)
            castle_perm += "Q";
        if (pos->GetCastlingPerm() & BKCA)
            castle_perm += "k";
        if (pos->GetCastlingPerm() & BQCA)
            castle_perm += "q";
    }
    // parse enpassant square
    if (GetEpSquare(pos) != no_sq) {
        ep_square = square_to_coordinates[GetEpSquare(pos)];
    } else {
        ep_square = "-";
    }

    // Parse fifty moves counter
    fifty_move = std::to_string(pos->Get50mrCounter());
    // Parse Hisply moves counter
    HisPly = std::to_string(pos->hisPly);

    return pos_string + " " + turn + " " + castle_perm + " " + ep_square + " " + fifty_move + " " + HisPly;
}

// parses the moves part of a fen string and plays all the moves included
void parse_moves(const std::string& moves, Position* pos) {
    std::vector<std::string> move_tokens = split_command(moves);
    // loop over moves within a move string
    for (size_t i = 0; i < move_tokens.size(); i++) {
        // parse next move
        const Move move = ParseMove(move_tokens[i], pos);
        // make move on the chess board
        MakeMove<false>(move, pos);
    }
    // don't rely on MakeUCIMove to update the position key
    pos->posKey = GeneratePosKey(pos);
}

// Returns the bitboard of a piecetype
Bitboard GetPieceBB(const Position* pos, const int piecetype) {
    return pos->GetPieceColorBB(piecetype, WHITE) | pos->GetPieceColorBB(piecetype, BLACK);
}

bool oppCanWinMaterial(const Position* pos, const int side) {
    Bitboard occ = pos->Occupancy(BOTH);
    Bitboard  us = pos->Occupancy(side);
    Bitboard oppPawns = pos->GetPieceColorBB(PAWN, side ^ 1);
    Bitboard ourPawns = pos->GetPieceColorBB(PAWN, side);
    while (oppPawns) {
        const int source_square = popLsb(oppPawns);
        if (pawn_attacks[side ^ 1][source_square] & (us ^ ourPawns))
            return true;
    }

    Bitboard oppKnights = pos->GetPieceColorBB(KNIGHT, side ^ 1);
    Bitboard ourKnights = pos->GetPieceColorBB(KNIGHT, side);
    Bitboard oppBishops = pos->GetPieceColorBB(BISHOP, side ^ 1);
    Bitboard ourBishops = pos->GetPieceColorBB(BISHOP, side);
    while (oppKnights) {
        const int source_square = popLsb(oppKnights);
        if (knight_attacks[source_square] & (us ^ ourPawns ^ ourKnights ^ ourBishops))
            return true;
    }

    while (oppBishops) {
        const int source_square = popLsb(oppBishops);
        if (GetBishopAttacks(source_square, occ) & (us ^ ourPawns ^ ourKnights ^ ourBishops))
            return true;
    }

    Bitboard oppRooks = pos->GetPieceColorBB(ROOK, side ^ 1);
    Bitboard ourRooks = pos->GetPieceColorBB(ROOK, side);
    while (oppRooks) {
        const int source_square = popLsb(oppRooks);
        if (GetRookAttacks(source_square, occ) & (us ^ ourPawns ^ ourKnights ^ ourBishops ^ ourRooks))
            return true;
    }

    return false;
}

Bitboard getThreats(const Position* pos, const int side) {
    // Take the occupancies of both positions, encoding where all the pieces on the board reside
    Bitboard occ = pos->Occupancy(BOTH);
    uint64_t threats = 0;

    // Get Pawn attacks
    Bitboard pawns = pos->GetPieceColorBB(PAWN, side);
    while (pawns) {
        int source_square = popLsb(pawns);
        threats |= pawn_attacks[side][source_square];
    }

    // Get Knight attacks
    Bitboard knights = pos->GetPieceColorBB(KNIGHT, side);
    while (knights) {
        int source_square = popLsb(knights);
        threats |= knight_attacks[source_square];
    }

    // Get Bishop attacks
    Bitboard bishops = pos->GetPieceColorBB(BISHOP, side);
    while (bishops) {
        int source_square = popLsb(bishops);
        threats |= GetBishopAttacks(source_square, occ);
    }
    // Get Rook attacks
    Bitboard rooks = pos->GetPieceColorBB(ROOK, side);
    while (rooks) {
        int source_square = popLsb(rooks);
        threats |= GetRookAttacks(source_square, occ);
    }
    // Get Queen attacks
    Bitboard queens = pos->GetPieceColorBB(QUEEN, side);
    while (queens) {
        int source_square = popLsb(queens);
        threats |= GetQueenAttacks(source_square, occ);
    }
    // Get King attacks
    Bitboard king = pos->GetPieceColorBB(KING, side);
    while (king) {
        int source_square = popLsb(king);
        threats |= king_attacks[source_square];
    }
    return threats;
}

// Return a piece based on the piecetype and the color
int GetPiece(const int piecetype, const int color) {
    return piecetype + 6 * color;
}

// Returns the piecetype of a piece
int GetPieceType(const int piece) {
    if (piece == EMPTY)
        return EMPTY;
    return PieceType[piece];
}

// Returns true if side has at least one piece on the board that isn't a pawn or the king, false otherwise
bool BoardHasNonPawns(const Position* pos, const int side) {
    return pos->Occupancy(side) ^ pos->GetPieceColorBB(PAWN, side) ^ pos->GetPieceColorBB(KING, side);
}

// Get on what square of the board the king of color c resides
int KingSQ(const Position* pos, const int c) {
    return GetLsbIndex(pos->GetPieceColorBB(KING, c));
}

void UpdatePinsAndCheckers(Position* pos, const int side) {
    const Bitboard them = pos->Occupancy(side ^ 1);
    const int kingSquare = KingSQ(pos, side);
    const Bitboard pawnCheckers = pos->GetPieceColorBB(PAWN, side ^ 1) & pawn_attacks[side][kingSquare];
    const Bitboard knightCheckers = pos->GetPieceColorBB(KNIGHT, side ^ 1) & knight_attacks[kingSquare];
    const Bitboard bishopsQueens = pos->GetPieceColorBB(BISHOP, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    const Bitboard rooksQueens = pos->GetPieceColorBB(ROOK, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    Bitboard sliderAttacks = (bishopsQueens & GetBishopAttacks(kingSquare, them)) | (rooksQueens & GetRookAttacks(kingSquare, them));
    pos->pinned = 0ULL;
    pos->checkers = pawnCheckers | knightCheckers;

    while (sliderAttacks) {
        const int sq = popLsb(sliderAttacks);
        const Bitboard blockers = (RayBetween(kingSquare, sq) | (1ULL << sq)) & pos->Occupancy(side);
        const int numBlockers = CountBits(blockers);
        if (!numBlockers)
            pos->checkers |= 1ULL << sq;
        else if (numBlockers == 1)
            pos->pinned |= blockers;
    }
}

Bitboard RayBetween(int square1, int square2) {
    return SQUARES_BETWEEN_BB[square1][square2];
}

int GetEpSquare(const Position* pos) {
    return pos->enPas;
}

// Calculates what the key for position pos will be after move <move>, it's a rough estimate and will fail for "special" moves such as promotions and castling
ZobristKey keyAfter(const Position* pos, const Move move) {

    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int  captured = pos->PieceOn(targetSquare);

    ZobristKey newKey = pos->GetPoskey() ^ SideKey ^ PieceKeys[piece][sourceSquare] ^ PieceKeys[piece][targetSquare];

    if (captured != EMPTY)
        newKey ^= PieceKeys[captured][targetSquare];

    return newKey;
}

void saveBoardState(Position* pos) {
    pos->history[pos->historyStackHead].fiftyMove = pos->fiftyMove;
    pos->history[pos->historyStackHead].enPas = pos->enPas;
    pos->history[pos->historyStackHead].castlePerm = pos->castleperm;
    pos->history[pos->historyStackHead].plyFromNull = pos->plyFromNull;
    pos->history[pos->historyStackHead].checkers = pos->checkers;
    pos->history[pos->historyStackHead].checkMask = pos->checkMask;
    pos->history[pos->historyStackHead].pinned = pos->pinned;
}

void restorePreviousBoardState(Position* pos)
{
    pos->enPas = pos->history[pos->historyStackHead].enPas;
    pos->fiftyMove = pos->history[pos->historyStackHead].fiftyMove;
    pos->castleperm = pos->history[pos->historyStackHead].castlePerm;
    pos->plyFromNull = pos->history[pos->historyStackHead].plyFromNull;
    pos->checkers = pos->history[pos->historyStackHead].checkers;
    pos->checkMask = pos->history[pos->historyStackHead].checkMask;
    pos->pinned = pos->history[pos->historyStackHead].pinned;
}

bool hasGameCycle(Position* pos, int ply) {

    int end = std::min(pos->Get50mrCounter(), pos->plyFromNull);

    if (end < 3)
        return false;

    // lamba function to return the zobrist key of a position that appeared i plies ago
    const auto OldKey = [pos](int i)
    {
        return pos->played_positions[pos->played_positions.size() - i];
    };

    const Bitboard occ = pos->Occupancy(BOTH);
    const ZobristKey originalKey = pos->posKey;
    ZobristKey other = (originalKey ^ OldKey(1));

    for (int i = 3; i <= end; i += 2)
    {
        ZobristKey currKey = OldKey(i);
        other ^= ~(currKey ^ OldKey(i-1));

        const auto diff = originalKey ^ currKey;
        uint32_t slot = H1(diff);
        if (diff != keys[slot])
            slot = H2(diff);

        if (diff != keys[slot])
            continue;

        const auto move = cuckooMoves[slot];

        if ((occ & RayBetween(From(move), To(move))) == 0ULL)
        {
            // repetition is after root, done
            if (ply > i)
                return true;

            auto piece = pos->PieceOn(From(move));
            if (piece == EMPTY)
                piece = pos->PieceOn(To(move));

            return Color[piece] == pos->side;
        }
    }
    return false;
}
