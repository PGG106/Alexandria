#include "Board.h"
#include "PieceData.h"
#include "hashkey.h"
#include "makemove.h"
#include "math.h"
#include "nnue.h"
#include "string.h"
#include <cassert>
#include <stdio.h>
#include "misc.h"
#include <iostream>

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

// convert squares to coordinates
const char* square_to_coordinates[] = {
	"a8", "b8", "c8", "d8", "e8", "f8","g8","h8",
	"a7", "b7", "c7","d7", "e7", "f7","g7", "h7",
	"a6", "b6", "c6", "d6", "e6", "f6","g6","h6",
	"a5", "b5", "c5", "d5", "e5", "f5","g5","h5",
	"a4", "b4", "c4", "d4", "e4", "f4","g4","h4",
	"a3", "b3", "c3", "d3","e3", "f3","g3", "h3",
	"a2", "b2", "c2", "d2", "e2", "f2","g2","h2",
	"a1", "b1", "c1", "d1", "e1", "f1","g1","h1",
};

//Lookup to get the rank of a square
const int get_rank[64] = { 7, 7, 7, 7, 7, 7, 7, 7,
						  6, 6, 6, 6, 6, 6, 6, 6,
						  5, 5, 5, 5, 5, 5, 5, 5,
						  4, 4, 4, 4, 4, 4, 4, 4,
						  3, 3, 3, 3, 3, 3, 3, 3,
						  2, 2, 2, 2, 2, 2, 2, 2,
						  1, 1, 1, 1, 1, 1, 1, 1,
						  0, 0, 0, 0, 0, 0, 0, 0 };

//Lookup to get the color from a piece
const int Color[12] = { WHITE, WHITE, WHITE, WHITE, WHITE, WHITE,
					   BLACK, BLACK, BLACK, BLACK, BLACK, BLACK };

const int PieceType[12] = { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

// castling rights update constants
const int castling_rights[64] = {
	7,  15, 15, 15, 3,  15, 15, 11, 15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 13, 15, 15, 15, 12, 15, 15, 14 };

NNUE nnue = NNUE();

int count_bits(Bitboard b) {
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)

	return (uint8_t)_mm_popcnt_u64(b);

#else // Assumed gcc or compatible compiler

	return __builtin_popcountll(b);

#endif
}

#if defined(__GNUC__) // GCC, Clang, ICC

int get_ls1b_index(Bitboard b) { return int(__builtin_ctzll(b)); }

#elif defined(_MSC_VER) // MSVC

#ifdef _WIN64 // MSVC, WIN64
#include <intrin.h>
// get least significant 1st bit index
int get_ls1b_index(Bitboard bitboard) {
	unsigned long idx;
	_BitScanForward64(&idx, bitboard);
	return (int)idx;
}

#else // MSVC, WIN32
#include <intrin.h>
int get_ls1b_index(Bitboard b) {
	assert(b);
	unsigned long idx;

	if (b & 0xffffffff) {
		_BitScanForward(&idx, int32_t(b));
		return int(idx);
	}
	else {
		_BitScanForward(&idx, int32_t(b >> 32));
		return int(idx + 32);
	}
}

#endif

#else // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif

//Reset the position to a clean state
void ResetBoard(S_Board* pos) {
	for (int index = 0; index < 64; ++index) {
		pos->pieces[index] = EMPTY;
	}

	pos->side = BOTH;
	pos->enPas = no_sq;
	pos->fiftyMove = 0;
	pos->ply = 0;        //number of plies in the current search instance
	pos->hisPly = 0;     // total number of halfmoves
	pos->castleperm = 0; // integer that represents the castling permission in his
	// bits (1111) = all castlings allowed (0000) no castling
	// allowed, (0101) only WKCA and BKCA allowed...
	pos->posKey = 0ULL;
	pos->pinD = 0;
	pos->pinHV = 0;
	pos->checkMask = 18446744073709551615ULL;
	pos->checks = 0;

	// set default nnue values
	for (size_t i = 0; i < HIDDEN_BIAS; i++) {
		pos->accumulator[i] = nnue.hiddenBias[i];
	}

	//Reset nnue accumulator stack
	pos->accumulatorStack.clear();
}

void Reset_info(S_SearchINFO* info) {
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
}

int square_distance(int a, int b) {
	return std::max(abs(get_file[a] - get_file[b]), abs(get_rank[a] - get_rank[b]));
}

// parse FEN string
void parse_fen(const std::string& command, S_Board* pos) {
	// reset board position (pos->pos->bitboards)
	memset(pos->bitboards, 0ULL, sizeof(pos->bitboards));

	// reset pos->occupancies (pos->pos->bitboards)
	memset(pos->occupancies, 0ULL, sizeof(pos->occupancies));

	ResetBoard(pos);

	std::vector<std::string> tokens = split_command(command);

	const std::string pos_string = tokens.at(0);
	const std::string turn = tokens.at(1);
	const std::string castle_perm = tokens.at(2);
	const std::string ep_square = tokens.at(3);
	std::string fifty_move;
	std::string HisPly;
	//Keep fifty move and Hisply arguments optional
	if (tokens.size() >= 5) {
		fifty_move = tokens.at(4);
		if (tokens.size() >= 6) {
			HisPly = tokens.at(5);
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
	//parse player turn
	(turn == "w") ? (pos->side = WHITE) : (pos->side = BLACK);

	//Parse castling rights
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

	//Read fifty moves counter
	if (!fifty_move.empty()) {
		pos->fiftyMove = std::stoi(fifty_move);
	}
	//Read Hisply moves counter
	if (!HisPly.empty()) {

		pos->hisPly = std::stoi(HisPly);

	}

	// loop over white pieces pos->pos->bitboards
	for (int piece = WP; piece <= WK; piece++)
		// populate white occupancy bitboard
		pos->occupancies[WHITE] |= pos->bitboards[piece];

	// loop over black pieces pos->pos->bitboards
	for (int piece = BP; piece <= BK; piece++)
		// populate white occupancy bitboard
		pos->occupancies[BLACK] |= pos->bitboards[piece];

	// init all pos->occupancies
	pos->occupancies[BOTH] |= pos->occupancies[WHITE];
	pos->occupancies[BOTH] |= pos->occupancies[BLACK];

	pos->posKey = GeneratePosKey(pos);

	//Update nnue accumulator to reflect board state
	accumulate(pos->accumulator, pos);

}

// parses the moves part of a fen string and plays all the moves included
void parse_moves(const std::string moves, S_Board* pos)
{
	std::vector<std::string> move_tokens = split_command(moves);
	// loop over moves within a move string
	for (size_t i = 0;i < move_tokens.size();i++) {
		// parse next move
		int move = parse_move(move_tokens[i], pos);
		// make move on the chess board
		make_move_light(move, pos);
	}
}


//Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
Bitboard GetPieceColorBB(const S_Board* pos, const int piecetype, const int color) {
	return pos->bitboards[piecetype + color * 6];
}
//Retrieve a generic piece (useful when we don't know what type of piece we are dealing with
Bitboard GetPieceBB(const S_Board* pos, const  int piecetype) {
	return GetPieceColorBB(pos, piecetype, WHITE) | GetPieceColorBB(pos, piecetype, BLACK);
}
//Return a piece based on the type and the color 
int GetPiece(const int piecetype, const int color) {
	return piecetype + 6 * color;
}

//Return a piece based on the type and the color 
int GetPieceType(const int piece) {
	return PieceType[piece];
}

//Returns true if side has at least one piece on the board that isn't a pawn, false otherwise
bool BoardHasNonPawns(const S_Board* pos, const int side) {
	return (Occupancy(pos, side) ^ GetPieceColorBB(pos, PAWN, side)) ^ GetPieceColorBB(pos, KING, side);
}

//Get on what square of the board the king of color c resides
int KingSQ(const S_Board* pos, const int c) {
	return (get_ls1b_index(GetPieceColorBB(pos, KING, c)));
}

bool IsInCheck(const S_Board* pos, const int side) {
	return is_square_attacked(pos, KingSQ(pos, side), side ^ 1);
}

int PieceOn(const S_Board* pos, const int square)
{
	return pos->pieces[square];
}

Bitboard Us(const S_Board* pos) {
	return pos->occupancies[pos->side];
}

Bitboard Enemy(const S_Board* pos) {
	return pos->occupancies[pos->side ^ 1];
}

Bitboard Occupancy(const S_Board* pos, int side) {
	return pos->occupancies[side];
}

int get_side(const S_Board* pos) {
	return pos->side;
}
int get_ep_square(const S_Board* pos) {
	return pos->enPas;
}
int get_fifty_moves_counter(const S_Board* pos) {
	return pos->fiftyMove;
}
int get_castleperm(const S_Board* pos) {
	return pos->castleperm;
}
int get_poskey(const S_Board* pos) {
	return pos->posKey;
}


void accumulate(NNUE::accumulator& board_accumulator, S_Board* pos) {

	for (int i = 0; i < HIDDEN_BIAS; i++) {
		board_accumulator[i] = nnue.hiddenBias[i];
	}

	for (int i = 0; i < 64; i++) {
		bool input = pos->pieces[i] != EMPTY;
		if (!input) continue;
		nnue.add(board_accumulator, pos->pieces[i], i);
	}
}
