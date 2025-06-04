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
    pos->history.head = 0;
    // reset board position (pos->pos->bitboards)
    std::memset(pos->state().bitboards, 0ULL, sizeof(pos->state().bitboards));

    // reset pos->occupancies (pos->pos->bitboards)
    std::memset(pos->state().occupancies, 0ULL, sizeof(pos->state().occupancies));

    for (int index = 0; index < 64; ++index) {
        pos->state().pieces[index] = EMPTY;
    }
    pos->state().castlePerm = 0;
    pos->state().plyFromNull = 0;
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
    if (pos->getEpSquare() != no_sq) {
        assert(pos->getEpSquare() >= 0 && pos->getEpSquare() < 64);
        finalkey ^= enpassant_keys[pos->getEpSquare()];
    }
    assert(pos->getCastlingPerm() >= 0 && pos->getCastlingPerm() <= 15);
    // add to the key the status of the castling permissions
    finalkey ^= CastleKeys[pos->getCastlingPerm()];
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

// Generates zobrist key (for non-pawns) from scratch
ZobristKey GenerateNonPawnKey(const Position* pos, int side) {
    Bitboard nonPawnKey = 0;
    for (int sq = 0; sq < 64; ++sq) {
        // get piece on that square
        const int piece = pos->PieceOn(sq);
        if (piece != EMPTY && piece != WP && piece != BP && Color[piece] == side) {
            nonPawnKey ^= PieceKeys[piece][sq];
        }
    }
    return nonPawnKey;
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
                    set_bit(pos->state().bitboards[piece], square);
                    pos->state().pieces[square] = piece;
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
            (pos->state().castlePerm) |= WKCA;
            break;
        case 'Q':
            (pos->state().castlePerm) |= WQCA;
            break;
        case 'k':
            (pos->state().castlePerm) |= BKCA;
            break;
        case 'q':
            (pos->state().castlePerm) |= BQCA;
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
        pos->state().enPas = rank * 8 + file;
    }
    // no enpassant square
    else
        pos->state().enPas = no_sq;

    // Read fifty moves counter
    if (!fifty_move.empty()) {
        pos->state().fiftyMove = std::stoi(fifty_move);
    }
    else {
        pos->state().fiftyMove = 0;
    }
    // Read Hisply moves counter
    if (!HisPly.empty()) {
        pos->state().hisPly = std::stoi(HisPly);
    }
    else {
        pos->state().hisPly = 0;
    }

    for (int piece = WP; piece <= WK; piece++)
        // populate white occupancy bitboard
        pos->state().occupancies[WHITE] |= pos->state().bitboards[piece];

    for (int piece = BP; piece <= BK; piece++)
        // populate white occupancy bitboard
        pos->state().occupancies[BLACK] |= pos->state().bitboards[piece];

    pos->state().posKey = GeneratePosKey(pos);
    pos->state().pawnKey = GeneratePawnKey(pos);
    pos->state().whiteNonPawnKey = GenerateNonPawnKey(pos, WHITE);
    pos->state().blackNonPawnKey = GenerateNonPawnKey(pos, BLACK);

    // Update pinmasks and checkers
    UpdatePinsAndCheckers(pos);

    // Clear vector of played positions
    pos->played_positions.clear();
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
    if (pos->getCastlingPerm() == 0)
        castle_perm = '-';
    else {
        if (pos->getCastlingPerm() & WKCA)
            castle_perm += "K";
        if (pos->getCastlingPerm() & WQCA)
            castle_perm += "Q";
        if (pos->getCastlingPerm() & BKCA)
            castle_perm += "k";
        if (pos->getCastlingPerm() & BQCA)
            castle_perm += "q";
    }
    // parse enpassant square
    if (pos->getEpSquare() != no_sq) {
        ep_square = square_to_coordinates[pos->getEpSquare()];
    } else {
        ep_square = "-";
    }

    // Parse fifty moves counter
    fifty_move = std::to_string(pos->get50MrCounter());
    // Parse Hisply moves counter
    HisPly = std::to_string(pos->state().hisPly);

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

void updatePinsSide(Position* pos, const int side){
    const Bitboard them = pos->Occupancy(side ^ 1);
    const int kingSquare = KingSQ(pos, side);
    const Bitboard bishopsQueens = pos->GetPieceColorBB(BISHOP, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    const Bitboard rooksQueens = pos->GetPieceColorBB(ROOK, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    Bitboard sliderAttacks = (bishopsQueens & GetBishopAttacks(kingSquare, them)) | (rooksQueens & GetRookAttacks(kingSquare, them));
    pos->state().pinned[side] = 0ULL;

    while (sliderAttacks) {
        const int sq = popLsb(sliderAttacks);
        const Bitboard blockers = RayBetween(kingSquare, sq) & pos->Occupancy(side);
        const int numBlockers = CountBits(blockers);
        if (numBlockers == 1)
            pos->state().pinned[side] |= blockers;
    }
}

void UpdatePinsAndCheckers(Position* pos) {

    const int side = pos->side;
    const Bitboard them = pos->Occupancy(side ^ 1);
    const int kingSquare = KingSQ(pos, side);
    const Bitboard pawnCheckers = pos->GetPieceColorBB(PAWN, side ^ 1) & pawn_attacks[side][kingSquare];
    const Bitboard knightCheckers = pos->GetPieceColorBB(KNIGHT, side ^ 1) & knight_attacks[kingSquare];
    const Bitboard bishopsQueens = pos->GetPieceColorBB(BISHOP, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    const Bitboard rooksQueens = pos->GetPieceColorBB(ROOK, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    Bitboard sliderAttacks = (bishopsQueens & GetBishopAttacks(kingSquare, them)) | (rooksQueens & GetRookAttacks(kingSquare, them));
    pos->state().checkers = pawnCheckers | knightCheckers;

    while (sliderAttacks) {
        const int sq = popLsb(sliderAttacks);
        const Bitboard blockers = RayBetween(kingSquare, sq) & pos->Occupancy(side);
        const int numBlockers = CountBits(blockers);
        if (!numBlockers)
            pos->state().checkers |= 1ULL << sq;
    }

    updatePinsSide(pos, WHITE);
    updatePinsSide(pos, BLACK);
}

Bitboard RayBetween(int square1, int square2) {
    return SQUARES_BETWEEN_BB[square1][square2];
}

// Calculates what the key for position pos will be after move <move>, it's a rough estimate and will fail for "special" moves such as promotions and castling
ZobristKey keyAfter(const Position* pos, const Move move) {

    const int sourceSquare = From(move);
    const int targetSquare = To(move);
    const int piece = Piece(move);
    const int  captured = pos->PieceOn(targetSquare);

    ZobristKey newKey = pos->getPoskey() ^ SideKey ^ PieceKeys[piece][sourceSquare] ^ PieceKeys[piece][targetSquare];

    if (captured != EMPTY)
        newKey ^= PieceKeys[captured][targetSquare];

    return 0;
}

bool hasGameCycle(Position* pos, int ply) {

    int end = std::min(pos->get50MrCounter(), pos->getPlyFromNull());

    if (end < 3)
        return false;

    // lamba function to return the zobrist key of a position that appeared i plies ago
    const auto OldKey = [pos](int i)
    {
        return pos->played_positions[pos->played_positions.size() - i];
    };

    const Bitboard occ = pos->Occupancy(BOTH);
    const ZobristKey originalKey = pos->getPoskey();
    ZobristKey other = (originalKey ^ OldKey(1) ^ SideKey);

    for (int i = 3; i <= end; i += 2)
    {
        ZobristKey currKey = OldKey(i);
        other ^= currKey ^ OldKey(i-1) ^ SideKey;

        if (other != 0)
            continue;

        const auto diff = originalKey ^ currKey;
        uint32_t slot = H1(diff);
        if (diff != keys[slot])
            slot = H2(diff);

        if (diff != keys[slot])
            continue;

        const auto move = cuckooMoves[slot];

        const int to = To(move);
        const int from = From(move);

        if (!((RayBetween(to, from) ^ 1ULL << to) & occ))
        {
            // repetition is after root, done
            if (ply > i)
                return true;

            auto piece = pos->PieceOn(from);
            if (piece == EMPTY)
                piece = pos->PieceOn(to);

            assert(piece != EMPTY);

            return Color[piece] == pos->side;
        }
    }
    return false;
}
