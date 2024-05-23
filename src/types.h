#pragma once
#include <cstdint>
// include the tune stuff here to give it global visibility
#include "tune.h"

#define NAME "Alexandria-6.1.14"

#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// define bitboard data type
using Bitboard = uint64_t;
// define poskey data type
using TTKey = uint16_t;
// define poskey data type
using ZobristKey = uint64_t;

constexpr int MAXPLY = 128;
constexpr int MAXDEPTH = MAXPLY;
constexpr int NOMOVE = 0;
constexpr int MATE_SCORE = 32000;
constexpr int MATE_FOUND = MATE_SCORE - MAXPLY;
constexpr int SCORE_NONE = 32001;
constexpr int MAXSCORE = 32670;
constexpr Bitboard fullCheckmask = 0xFFFFFFFFFFFFFFFF;

// board squares
enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq
};

// Lookup to get the rank of a square
constexpr int get_rank[64] = { 7, 7, 7, 7, 7, 7, 7, 7,
                               6, 6, 6, 6, 6, 6, 6, 6,
                               5, 5, 5, 5, 5, 5, 5, 5,
                               4, 4, 4, 4, 4, 4, 4, 4,
                               3, 3, 3, 3, 3, 3, 3, 3,
                               2, 2, 2, 2, 2, 2, 2, 2,
                               1, 1, 1, 1, 1, 1, 1, 1,
                               0, 0, 0, 0, 0, 0, 0, 0 };

// Lookup to get the file of a square
constexpr int get_file[64] = { 0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7 };

// Lookup to get the diagonal of a square
constexpr int get_diagonal[64] = { 14, 13, 12, 11, 10,  9,  8,  7,
                                   13, 12, 11, 10,  9,  8,  7,  6,
                                   12, 11, 10,  9,  8,  7,  6,  5,
                                   11, 10,  9,  8,  7,  6,  5,  4,
                                   10,  9,  8,  7,  6,  5,  4,  3,
                                    9,  8,  7,  6,  5,  4,  3,  2,
                                    8,  7,  6,  5,  4,  3,  2,  1,
                                    7,  6,  5,  4,  3,  2,  1,  0 };

// encode pieces
enum {
    WP, WN, WB, WR, WQ, WK,
    BP, BN, BB, BR, BQ, BK,
    EMPTY = 14
};

// sides to move (colors)
enum {
    WHITE,
    BLACK,
    BOTH
};

enum {
    WKCA = 1,
    WQCA = 2,
    BKCA = 4,
    BQCA = 8
};

enum {
    HFNONE,
    HFUPPER,
    HFLOWER,
    HFEXACT
};

// piece types
enum {
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
};

// Lookup to get the color from a piece
constexpr int Color[12] = { WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
                            BLACK, BLACK, BLACK, BLACK, BLACK, BLACK };

constexpr int PieceType[12] = { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
                                PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

// Contains the material Values of the pieces
constexpr int SEEValue[15] = { 100, 422, 422, 642, 1015, 0,
                               100, 422, 422, 642, 1015, 0, 0, 0, 0 };

