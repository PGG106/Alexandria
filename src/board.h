#pragma once

#include <cassert>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include "nnue.h"
#include "stdint.h"
#include "move.h"
#include "types.h"

#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__) \
    __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

// set/get/pop bit macros
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
#define pop_lsb(bitboard) ((bitboard) &= (bitboard - 1))

#define get_antidiagonal(sq) (get_rank[sq] + get_file[sq])

#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// Lookup to get the rank of a square
constexpr int get_rank[64] = { 7, 7, 7, 7, 7, 7, 7, 7,
                               6, 6, 6, 6, 6, 6, 6, 6,
                               5, 5, 5, 5, 5, 5, 5, 5,
                               4, 4, 4, 4, 4, 4, 4, 4,
                               3, 3, 3, 3, 3, 3, 3, 3,
                               2, 2, 2, 2, 2, 2, 2, 2,
                               1, 1, 1, 1, 1, 1, 1, 1,
                               0, 0, 0, 0, 0, 0, 0, 0 };

// extract rank from a square [square]
constexpr int get_file[64] = { 0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7,
                               0, 1, 2, 3, 4, 5, 6, 7 };

// extract diagonal from a square [square]
constexpr int get_diagonal[Board_sq_num] = { 14, 13, 12, 11, 10,  9,  8,  7, 
                                             13, 12, 11, 10,  9,  8,  7,  6, 
                                             12, 11, 10,  9,  8,  7,  6,  5, 
                                             11, 10,  9,  8,  7,  6,  5,  4, 
                                             10,  9,  8,  7,  6,  5,  4,  3, 
                                              9,  8,  7,  6,  5,  4,  3,  2,  
                                              8,  7,  6,  5,  4,  3,  2,  1, 
                                              7,  6,  5,  4,  3,  2,  1,  0 };

// Lookup to get the color from a piece
constexpr int Color[12] = { WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
                            BLACK, BLACK, BLACK, BLACK, BLACK, BLACK };

constexpr int PieceType[12] = { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
                                PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

extern int reductions[2][MAXDEPTH][MAXPLY];
extern int lmp_margin[MAXDEPTH][2];
extern int see_margin[MAXDEPTH][2];

struct S_Undo {
    int castlePerm = 15;
    int capture = EMPTY;
    int enPas = 0;
    int fiftyMove = 0;
    int plyFromNull = 0;
    Bitboard checkers = 0ULL;
    Bitboard checkMask = fullCheckmask;
}; // stores a move and the state of the game before that move is made
// for rollback purposes

// counts how many bits are set in a bitboard
int CountBits(Bitboard bitboard);

struct S_Board {
public:
    int pieces[Board_sq_num]; // array that stores for every square of the board
    // if there's a piece, or if the square is invalid

    int side = -1; // what side has to move
    int enPas = no_sq; // if enpassant is possible and in which square
    int fiftyMove = 0; // Counter for the 50 moves rule
    int hisPly = 0; // total number of halfmoves
    int plyFromNull = 0;
    int castleperm = 0;
    // unique  hashkey  that encodes a board position
    ZobristKey posKey = 0ULL;
    // stores the state of the board  rollback purposes
    S_Undo    history[1024];
    // Stores the zobrist keys of all the positions played in the game + the current search instance, used for 3-fold
    std::vector<ZobristKey> played_positions = {};
    Bitboard pinHV = 0ULL;
    Bitboard pinD = 0ULL;

    // Occupancies bitboards based on piece and side
    Bitboard bitboards[12] = {};
    Bitboard occupancies[2] = {};
    NNUE::accumulator accumulator = {};
    // Previous values of the nnue accumulators. always empty at the start of search
    std::vector<NNUE::accumulator> accumulatorStack = {};
    Bitboard checkers;
    Bitboard checkMask = fullCheckmask;
  
    NNUE::accumulator accumStack[256];
    int accumStackHead;

    inline NNUE::accumulator& AccumulatorTop() {
        return accumStack[accumStackHead-1];
    }

    inline Bitboard Us() const {
        return occupancies[side];
    }

    inline Bitboard Enemy() const {
        return occupancies[side ^ 1];
    }

    inline Bitboard Occupancy(const int occupancySide) const {
        assert(occupancySide >= WHITE && occupancySide <= BOTH);
        if (occupancySide == BOTH)
            return occupancies[WHITE] | occupancies[BLACK];
        else
            return occupancies[occupancySide];
    }

    // Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
    inline Bitboard GetPieceColorBB(const int piecetype, const int color) const {
        return bitboards[piecetype + color * 6];
    }

    inline int PieceCount() const {
        return CountBits(Occupancy(BOTH));
    }

    inline int PieceOn(const int square) const {
        assert(square >= 0 && square <= 63);
        return pieces[square];
    }

    inline int GetSide() const {
        return side;
    }

    inline ZobristKey GetPoskey() const {
        return posKey;
    }

    inline int Get50mrCounter() const {
        return fiftyMove;
    }

    inline int GetCastlingPerm() const {
        return castleperm;
    }

    inline void ChangeSide() {
        side ^= 1;
    }
};

struct PvTable {
    int pvLength[MAXDEPTH + 1];
    int pvArray[MAXDEPTH + 1][MAXDEPTH + 1];
};

extern Bitboard SQUARES_BETWEEN_BB[Board_sq_num][Board_sq_num];
// Hold the data from the uci input to set search parameters and some search data to populate the uci output
struct S_SearchINFO {
    // search start time
    uint64_t starttime = 0;
    // search time initial lower bound if present
    uint64_t stoptimeBaseOpt = 0;
    // search time scaled lower bound if present
    uint64_t stoptimeOpt = 0;
    // search time upper bound if present
    uint64_t stoptimeMax = 0;
    // max depth to reach for depth limited searches
    int depth = -1;
    int seldepth = -1;
    // types of search limits
    bool timeset = false;
    bool nodeset = false;
    bool movetimeset = false;

    int movestogo = -1;
    uint64_t nodes = 0;
    uint64_t nodeslimit = 0;
    bool infinite = false;

    bool stopped = false;
};

// castling rights update constants
constexpr int castling_rights[64] = {
    7,  15, 15, 15, 3,  15, 15, 11, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 13, 15, 15, 15, 12, 15, 15, 14 };

// convert squares to coordinates
const constexpr char* square_to_coordinates[] = {
    "a8", "b8", "c8", "d8", "e8", "f8","g8","h8",
    "a7", "b7", "c7","d7", "e7", "f7","g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6","g6","h6",
    "a5", "b5", "c5", "d5", "e5", "f5","g5","h5",
    "a4", "b4", "c4", "d4", "e4", "f4","g4","h4",
    "a3", "b3", "c3", "d3","e3", "f3","g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2","g2","h2",
    "a1", "b1", "c1", "d1", "e1", "f1","g1","h1",
};

// ASCII pieces
constexpr char ascii_pieces[13] = "PNBRQKpnbrqk";

// NNUE
extern NNUE nnue;

// get least significant 1st bit index
[[nodiscard]] int GetLsbIndex(Bitboard bitboard);

[[nodiscard]] int SquareDistance(int a, int b);

// parse FEN string
void ParseFen(const std::string& command, S_Board* pos);
// Get fen string from board
[[nodiscard]] std::string GetFen(const S_Board* pos);
// Parse a string of moves in coordinate format and plays them
void parse_moves(const std::string& moves, S_Board* pos);

void ResetInfo(S_SearchINFO* info);

// Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
[[nodiscard]] Bitboard GetPieceBB(const S_Board* pos, const int piecetype);
// Returns the threats bitboard of the pieces of <side> color
[[nodiscard]] Bitboard getThreats(const S_Board* pos, const int side);
// Return a piece based on the type and the color
[[nodiscard]] int GetPiece(const int piecetype, const int color);
// Returns the piece_type of a piece
[[nodiscard]] int GetPieceType(const int piece);
// Returns true if side has at least one piece on the board that isn't a pawn, false otherwise
[[nodiscard]] bool BoardHasNonPawns(const S_Board* pos, const int side);
// Get on what square of the board the king of color c resides
[[nodiscard]] int KingSQ(const S_Board* pos, const int c);
[[nodiscard]] Bitboard GetCheckersBB(const S_Board* pos, const int side);
Bitboard RayBetween(int square1, int square2);
[[nodiscard]] int GetEpSquare(const S_Board* pos);
[[nodiscard]] uint64_t GetMaterialValue(const S_Board* pos);
void Accumulate(NNUE::accumulator& board_accumulator, S_Board* pos);
