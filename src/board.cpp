#include "board.h"
#include "piece_data.h"
#include "hashkey.h"
#include "makemove.h"
#include "movegen.h"
#include "misc.h"
#include "uci.h"
#include "attack.h"
#include "magic.h"
#include "io.h"
#include <cassert>

#if defined(_WIN64) && defined(_MSC_VER) // No Makefile used
#include <intrin.h> // Microsoft header for _BitScanForward64()
#define IS_64BIT
#endif
#if defined(USE_POPCNT) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif

#if !defined(NO_PREFETCH) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#include <xmmintrin.h> // Intel and Microsoft header for _mm_prefetch()
#endif
#include <iostream>

NNUE nnue = NNUE();

int CountBits(Bitboard bitboard) {
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
    return (uint8_t)_mm_popcnt_u64(bitboard);
#else // Assumed gcc or compatible compiler
    return __builtin_popcountll(bitboard);
#endif
}

int GetLsbIndex(Bitboard bitboard) {
#if defined(__GNUC__) // GCC, Clang, ICC
    return int(__builtin_ctzll(bitboard));
#elif defined(_MSC_VER) // MSVC
#ifdef _WIN64 // MSVC, WIN64
    unsigned long idx;
    _BitScanForward64(&idx, bitboard);
    return (int)idx;
#else // MSVC, WIN32
    assert(bitboard);
    unsigned long idx;

    if (b & 0xffffffff) {
        _BitScanForward(&idx, int32_t(bitboard));
        return int(idx);
    }
    else {
        _BitScanForward(&idx, int32_t(bitboard >> 32));
        return int(idx + 32);
    }
#endif
#else // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif
}

// Reset the position to a clean state
void ResetBoard(S_Board* pos) {
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

void ResetInfo(S_SearchINFO* info) {
    info->depth = 0;
    info->nodes = 0;
    info->starttime = 0;
    info->stoptimeOpt = 0;
    info->stoptimeMax = 0;
    info->infinite = 0;
    info->movestogo = -1;
    info->stopped = false;
    info->timeset = false;
    info->movetimeset = false;
    info->nodeset = false;
}

int SquareDistance(int a, int b) {
    return std::max(abs(get_file[a] - get_file[b]), abs(get_rank[a] - get_rank[b]));
}

// parse FEN string
void ParseFen(const std::string& command, S_Board* pos) {
    // Reset nnue accumulator stack
    pos->accumulatorStack.clear();

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

                // define piece variable
                int piece = -1;

                // loop over all piece pos->pos->bitboards
                for (int bb_piece = WP; bb_piece <= BK; bb_piece++) {
                    // if there is a piece on current square
                    if (get_bit(pos->bitboards[bb_piece], square))
                        // get piece code
                        piece = bb_piece;
                }
                // on empty current square
                if (piece == -1)
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

    pos->checkers = GetCheckersBB(pos, pos->side);

    // Update nnue accumulator to reflect board state
    Accumulate(pos->accumulator, pos);
}

std::string GetFen(const S_Board* pos) {
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
void parse_moves(const std::string& moves, S_Board* pos) {
    std::vector<std::string> move_tokens = split_command(moves);
    // loop over moves within a move string
    for (size_t i = 0; i < move_tokens.size(); i++) {
        // parse next move
        int move = ParseMove(move_tokens[i], pos);
        // make move on the chess board
        MakeMove(move, pos);
    }
}

// Returns the bitboard of a piecetype
Bitboard GetPieceBB(const S_Board* pos, const int piecetype) {
    return pos->GetPieceColorBB(piecetype, WHITE) | pos->GetPieceColorBB(piecetype, BLACK);
}

Bitboard getThreats(const S_Board* pos, const int side) {
    // Take the occupancies of both positions, encoding where all the pieces on the board reside
    Bitboard occ = pos->Occupancy(BOTH);
    uint64_t threats = 0;

    // Get Pawn attacks
    Bitboard pawns = pos->GetPieceColorBB(PAWN, pos->side);
    while (pawns) {
        int source_square = GetLsbIndex(pawns);
        threats |= pawn_attacks[side][source_square];
        pop_bit(pawns, source_square);
    }

    // Get Knight attacks
    Bitboard knights = pos->GetPieceColorBB(KNIGHT, pos->side);
    while (knights) {
        int source_square = GetLsbIndex(knights);
        threats |= knight_attacks[source_square];
        pop_bit(knights, source_square);
    }

    // Get Bishop attacks
    Bitboard bishops = pos->GetPieceColorBB(BISHOP, pos->side);
    while (bishops) {
        int source_square = GetLsbIndex(bishops);
        threats |= GetBishopAttacks(source_square, occ);
        pop_bit(bishops, source_square);
    }
    // Get Rook attacks
    Bitboard rooks = pos->GetPieceColorBB(ROOK, pos->side);
    while (rooks) {
        int source_square = GetLsbIndex(rooks);
        threats |= GetRookAttacks(source_square, occ);
        pop_bit(rooks, source_square);
    }
    // Get Queen attacks
    Bitboard queens = pos->GetPieceColorBB(QUEEN, pos->side);
    while (queens) {
        int source_square = GetLsbIndex(queens);
        threats |= GetQueenAttacks(source_square, occ);
        pop_bit(queens, source_square);
    }
    // Get King attacks
    Bitboard king = pos->GetPieceColorBB(KING, pos->side);
    while (king) {
        int source_square = GetLsbIndex(king);
        threats |= king_attacks[source_square];
        pop_bit(king, source_square);
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
bool BoardHasNonPawns(const S_Board* pos, const int side) {
    return pos->Occupancy(side) ^ pos->GetPieceColorBB(PAWN, side) ^ pos->GetPieceColorBB(KING, side);
}

// Get on what square of the board the king of color c resides
int KingSQ(const S_Board* pos, const int c) {
    return (GetLsbIndex(pos->GetPieceColorBB( KING, c)));
}

Bitboard GetCheckersBB(const S_Board* pos, const int side) {
    Bitboard Occ = pos->Occupancy(BOTH);
    int kingSquare = KingSQ(pos, side);
    // Bitboard of pawns that attack the king square
    const Bitboard pawn_mask = pos->GetPieceColorBB(PAWN, side ^ 1) & pawn_attacks[side][kingSquare];
    // Bitboard of knights that attack the king square
    const Bitboard knight_mask = pos->GetPieceColorBB(KNIGHT, side ^ 1) & knight_attacks[kingSquare];
    // Bitboard of bishops and queens that diagonally attack the king square
    const Bitboard bishopsQueens = pos->GetPieceColorBB(BISHOP, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    const Bitboard bishop_mask = bishopsQueens & GetBishopAttacks(kingSquare, Occ) & ~pos->occupancies[side];
    // Bitboard of rooks and queens that attack the king square
    const Bitboard rooksQueens = pos->GetPieceColorBB(ROOK, side ^ 1) | pos->GetPieceColorBB(QUEEN, side ^ 1);
    const Bitboard rook_mask = rooksQueens & GetRookAttacks(kingSquare, Occ) & ~pos->occupancies[side];
    
    Bitboard checkers = pawn_mask | knight_mask;

    if (bishop_mask) {
        Bitboard checkingBishops = bishop_mask;
        while (checkingBishops) {
        int index = GetLsbIndex(checkingBishops);
        checkers |= (1ULL << index);
        pop_bit(checkingBishops,index);
        }
    }
    if (rook_mask) {
        Bitboard checkingrook = rook_mask;
        while (checkingrook) {
            int index = GetLsbIndex(checkingrook);
            checkers |= (1ULL << index);
            pop_bit(checkingrook, index);
        }
        int index = GetLsbIndex(rook_mask);
        checkers |= (1ULL << index);
    }

    return checkers;
}

Bitboard RayBetween(int square1, int square2) {
    return SQUARES_BETWEEN_BB[square1][square2];
}

int GetEpSquare(const S_Board* pos) {
    return pos->enPas;
}

uint64_t GetMaterialValue(const S_Board* pos) {
    int pawns = CountBits(GetPieceBB(pos, PAWN));
    int knights = CountBits(GetPieceBB(pos, KNIGHT));
    int bishops = CountBits(GetPieceBB(pos, BISHOP));
    int rooks = CountBits(GetPieceBB(pos, ROOK));
    int queens = CountBits(GetPieceBB(pos, QUEEN));

    return pawns * PieceValue[PAWN] + knights * PieceValue[KNIGHT] + bishops * PieceValue[BISHOP] + rooks * PieceValue[ROOK] + queens * PieceValue[QUEEN];
}

void Accumulate(NNUE::accumulator& board_accumulator, S_Board* pos) {
    for (int i = 0; i < HIDDEN_SIZE; i++) {
        board_accumulator[0][i] = net.featureBias[i];
        board_accumulator[1][i] = net.featureBias[i];
    }

    for (int i = 0; i < 64; i++) {
        bool input = pos->pieces[i] != EMPTY;
        if (!input) continue;
        nnue.add(board_accumulator, pos->pieces[i], i);
    }
}
