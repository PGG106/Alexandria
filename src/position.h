#pragma once

#include <cassert>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include "bitboard.h"
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
#define get_antidiagonal(sq) (get_rank[sq] + get_file[sq])

extern int reductions[2][MAXDEPTH][MAXPLY];
extern int lmp_margin[MAXDEPTH][2];
extern int see_margin[MAXDEPTH][2];

struct S_boardState {
    int castlePerm = 15;
    int capture = EMPTY;
    int enPas = 0;
    int fiftyMove = 0;
    int plyFromNull = 0;
    Bitboard checkers = 0ULL;
    Bitboard checkMask = fullCheckmask;
    Bitboard pinned;
}; // stores a move and the state of the game before that move is made
// for rollback purposes

struct Position {
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
    int historyStackHead = 0;
    S_boardState    history[MAXPLY];
    // Stores the zobrist keys of all the positions played in the game + the current search instance, used for 3-fold
    std::vector<ZobristKey> played_positions = {};
    std::vector<NNUEIndices> NNUEAdd = {};
    std::vector<NNUEIndices> NNUESub = {};
    Bitboard pinned;

    // Occupancies bitboards based on piece and side
    Bitboard bitboards[12] = {};
    Bitboard occupancies[2] = {};
    Bitboard checkers;
    Bitboard checkMask = fullCheckmask;
  
    NNUE::accumulator accumStack[MAXPLY];
    int accumStackHead;

    inline NNUE::accumulator& AccumulatorTop() {
        assert(accumStackHead <= MAXPLY);
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
     7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14,
};

// convert squares to coordinates
const constexpr char* square_to_coordinates[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

// ASCII pieces
constexpr char ascii_pieces[13] = "PNBRQKpnbrqk";

// NNUE
extern NNUE nnue;

// Generates zobrist key from scratch
[[nodiscard]] Bitboard GeneratePosKey(const Position* pos);
// parse FEN string
void ParseFen(const std::string& command, Position* pos);
// Get fen string from board
[[nodiscard]] std::string GetFen(const Position* pos);
// Parse a string of moves in coordinate format and plays them
void parse_moves(const std::string& moves, Position* pos);

void ResetInfo(S_SearchINFO* info);

// Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
[[nodiscard]] Bitboard GetPieceBB(const Position* pos, const int piecetype);
// Returns the threats bitboard of the pieces of <side> color
[[nodiscard]] Bitboard getThreats(const Position* pos, const int side);
// Returns whether the opponent of <side> has a guaranteed SEE > 0
[[nodiscard]] bool oppCanWinMaterial(const Position* pos, const int side);
// Return a piece based on the type and the color
[[nodiscard]] int GetPiece(const int piecetype, const int color);
// Returns the piece_type of a piece
[[nodiscard]] int GetPieceType(const int piece);
// Returns true if side has at least one piece on the board that isn't a pawn, false otherwise
[[nodiscard]] bool BoardHasNonPawns(const Position* pos, const int side);
// Get on what square of the board the king of color c resides
[[nodiscard]] int KingSQ(const Position* pos, const int c);
void UpdatePinsAndCheckers(Position* pos, const int side);
Bitboard RayBetween(int square1, int square2);
[[nodiscard]] int GetEpSquare(const Position* pos);
[[nodiscard]] uint64_t GetMaterialValue(const Position* pos);
void Accumulate(NNUE::accumulator& board_accumulator, Position* pos);
ZobristKey keyAfter(const Position* pos, const int move);
void saveBoardState(Position* pos);
void restorePreviousBoardState(Position* pos);
