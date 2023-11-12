#pragma once

#include <cstdint>

#define NAME "Alexandria-5.0.20"

// define bitboard data type
using Bitboard = uint64_t;
// define poskey data type
using TTKey = uint32_t;
// define poskey data type
using ZobristKey = uint64_t;

constexpr int MAXPLY = 128;
constexpr int MAXDEPTH = MAXPLY;
constexpr int Board_sq_num = 64;
constexpr int NOMOVE = 0;
constexpr int mate_score = 32000;
constexpr int mate_found = mate_score - MAXPLY;
constexpr int score_none = 32001;
constexpr int MAXSCORE = 32670;

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

enum {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
    FILE_NONE
};

enum {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
    RANK_NONE
};

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

// Contains the material Values of the pieces
constexpr int PieceValue[15] = { 100, 300, 300, 450, 900, 0,
                      100, 300, 300, 450, 900, 0,0,0,0 };

enum {
    queenPromotionScore = 2000000001,
    knightPromotionScore = 2000000000,
    goodCaptureScore = 900000000,
    killerMoveScore0 = 800000000,
    killerMoveScore1 = 700000000,
    counterMoveScore= 600000000,
    badCaptureScore = -1000000,
    badPromotionScore = -2000000001
};
