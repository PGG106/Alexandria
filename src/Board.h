#pragma once
#include <cassert>
#include <cctype>
#include <string>
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

constexpr int MAXGAMEMOVES = 1024;
constexpr int MAXDEPTH = 128;
constexpr int UNDOSIZE = MAXGAMEMOVES + MAXDEPTH;
constexpr int Board_sq_num = 64;


// set/get/pop bit macros
#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
#define clr_bit(bitboard) ((bitboard) &= (bitboard - 1))

#define get_antidiagonal(sq) (get_rank[sq] + get_file[sq])

#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// extract rank from a square [square]
extern const int get_rank[Board_sq_num];

extern const int Color[12];

// extract rank from a square [square]
extern const int get_file[Board_sq_num];

// extract diagonal from a square [square]
extern const int get_diagonal[Board_sq_num];

extern int reductions[MAXDEPTH];

typedef struct Undo {
	int move = 0;
	int castlePerm = 15;
	int capture = EMPTY;
	int enPas = 0;
	int fiftyMove = 0;
	PosKey posKey = 0ULL;
	Bitboard occupancies[3];
} S_Undo; // stores a move and the state of the game before that move is made
// for rollback purposes

struct S_Board {
public:
	int pieces[Board_sq_num]; // array that stores for every square of the board
	// if there's a piece, or if the square is invalid

	int side = -1; // what side has to move
	int enPas = no_sq; // if enpassant is possible and in which square
	int fiftyMove = 0; // Counter for the 50 moves rule
	int ply = 0; // number of halfmoves in a search instance
	int hisPly = 0; // total number of halfmoves
	int castleperm = 0; // integer that represents the castling permission in its bits (1111) = all castlings allowed (0000) no castling

	PosKey posKey = 0ULL; // unique  hashkey  that encodes a board position

	S_Undo	history[UNDOSIZE]; // stores every single move and the state of the board when that move was made for rollback purposes

	Bitboard pinHV = 0ULL;
	Bitboard pinD = 0ULL;
	Bitboard checkMask = 0ULL;

	// Occupancies bitboards based on piece and side
	Bitboard bitboards[12];
	Bitboard occupancies[3];
	NNUE::accumulator accumulator = {};
	//Previous values of the nnue accumulators. always empty at the start of search
	std::vector<std::array<int16_t, HIDDEN_BIAS>> accumulatorStack = {};

	int checks = -1;
};


typedef struct Stack {
	int pvLength[MAXDEPTH + 1];
	int pvArray[MAXDEPTH + 1][MAXDEPTH + 1];
	int searchHistory[12][Board_sq_num] = { 0 };
	int searchKillers[2][MAXDEPTH] = { NOMOVE };
	int excludedMoves[MAXDEPTH] = { NOMOVE };
	int CounterMoves[Board_sq_num][Board_sq_num] = { 0 };
	int eval[MAXGAMEMOVES + MAXDEPTH] = { 0 };
} S_Stack;

extern Bitboard SQUARES_BETWEEN_BB[Board_sq_num][Board_sq_num];

typedef struct info {
	long starttime = 0;
	long stoptimeOpt = 0;
	long stoptimeMax = 0;
	int depth = -1;
	int seldepth = -1;
	bool timeset = false;
	bool nodeset = false;
	int movestogo = -1;
	uint64_t nodes = 0;
	uint64_t nodeslimit = 0;
	bool infinite = false;

	bool stopped = false;


} S_SearchINFO;

// castling rights update constants
extern const int castling_rights[Board_sq_num];

// convert squares to coordinates
extern const char* square_to_coordinates[];

extern char RankChar[];
extern char FileChar[];

// ASCII pieces
extern const char ascii_pieces[13];

// NNUE
extern NNUE nnue;

// counts how many bits are set in a bitboard
int count_bits(Bitboard bitboard);

// get least significant 1st bit index
int get_ls1b_index(Bitboard bitboard);
int square_distance(int a, int b);

// parse FEN string
void parse_fen(const std::string& command, S_Board* pos);
//Parse a string of moves in coordinate format and plays them
void parse_moves(std::string moves, S_Board* pos);

void Reset_info(S_SearchINFO* info);

//Pieces info retrival

//Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
Bitboard GetPieceColorBB(const S_Board* pos, const int piecetype, const  int color);
//Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
Bitboard GetPieceBB(const S_Board* pos, const int piecetype);
//Return a piece based on the type and the color 
int GetPiece(const int piecetype, const int color);
//Returns the piece_type of a piece
int GetPieceType(const int piece);
//Returns true if side has at least one piece on the board that isn't a pawn, false otherwise
bool BoardHasNonPawns(const S_Board* pos, const int side);
//Get on what square of the board the king of color c resides
int KingSQ(const S_Board* pos, const int c);
// returns if the current side is in check
bool IsInCheck(const S_Board* pos, const int side);
int PieceOn(const S_Board* pos, const int square);
//Occupancy info retrieval
Bitboard Us(const S_Board* pos);
Bitboard Enemy(const S_Board* pos);
Bitboard Occupancy(const S_Board* pos, const int side);


