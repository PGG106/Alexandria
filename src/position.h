#pragma once

#include <cassert>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include "bitboard.h"
#include "nnue.h"
#include <cstdint>
#include "move.h"
#include "types.h"
#include "uci.h"

#ifdef __GNUC__
#define PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK(__Declaration__) \
    __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif
#define get_antidiagonal(sq) (get_rank[sq] + get_file[sq])

struct BoardState {
    int pieces[64];
    // Occupancies bitboards based on piece and side
    Bitboard bitboards[12] = {};
    Bitboard occupancies[2] = {};
    int castlePerm = 15;
    int enPas = 0;
    int fiftyMove = 0;
    int plyFromNull = 0;
    Bitboard checkers = 0ULL;
    Bitboard pinned;
    ZobristKey pawnKey = 0ULL;
    ZobristKey whiteNonPawnKey = 0ULL;
    ZobristKey blackNonPawnKey = 0ULL;
}; // stores a move and the state of the game before that move is made
// for rollback purposes

struct historyStack{
    BoardState    historyStack[MAXPLY];
    int head = 0;

    inline void push(BoardState state) {
        historyStack[head] = state;
        head++;
    }

    inline BoardState pop() {
        return historyStack[--head];
    }

};

struct Position {
public:
    int side = -1; // what side has to move

    BoardState state;
    int hisPly = 0; // total number of halfmoves
    // unique  hashkey  that encodes a board position
    ZobristKey posKey = 0ULL;

    // stores the state of the board  rollback purposes
    historyStack history;
    // Stores the zobrist keys of all the positions played in the game + the current search instance, used for 3-fold
    std::vector<ZobristKey> played_positions = {};

    [[nodiscard]] inline Bitboard Occupancy(const int occupancySide) const {
        assert(occupancySide >= WHITE && occupancySide <= BOTH);
        if (occupancySide == BOTH)
            return state.occupancies[WHITE] | state.occupancies[BLACK];
        else
            return state.occupancies[occupancySide];
    }

    // Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
    [[nodiscard]] inline Bitboard GetPieceColorBB(const int piecetype, const int color) const {
        return state.bitboards[piecetype + color * 6];
    }

    [[nodiscard]] inline int PieceCount() const {
        return CountBits(Occupancy(BOTH));
    }

    [[nodiscard]] inline int PieceOn(const int square) const {
        assert(square >= 0 && square <= 63);
        return state.pieces[square];
    }

    [[nodiscard]] inline ZobristKey getPoskey() const {
        return posKey;
    }

    [[nodiscard]] inline int get50MrCounter() const {
        return state.fiftyMove;
    }

    [[nodiscard]] inline int getCastlingPerm() const {
        return state.castlePerm;
    }

    [[nodiscard]] inline int getEpSquare() const {
        return state.enPas;
    }

    [[nodiscard]] inline int getPlyFromNull() const {
        return state.plyFromNull;
    }

    [[nodiscard]] inline Bitboard getCheckers() const {
        return state.checkers;
    }

    [[nodiscard]] inline Bitboard getPinnedMask() const {
        return state.pinned;
    }

    inline void ChangeSide() {
        side ^= 1;
    }
};

extern Bitboard SQUARES_BETWEEN_BB[64][64];

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

[[nodiscard]] ZobristKey GeneratePosKey(const Position* pos);
[[nodiscard]] ZobristKey GeneratePawnKey(const Position* pos);
// parse FEN string
void ParseFen(const std::string& command, Position* pos);
// Get fen string from board
[[nodiscard]] std::string GetFen(const Position* pos);
// Parse a string of moves in coordinate format and plays them
void parse_moves(const std::string& moves, Position* pos);

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

ZobristKey keyAfter(const Position* pos, const Move move);

bool hasGameCycle(Position* pos, int ply);
