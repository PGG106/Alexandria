#include "Board.h"
#include "eval.h"
#include "magic.h"
#include "stdio.h"
#include "stdlib.h"
#include "io.h"
#include "PieceData.h"




// material score [game phase][piece]
const int material_score[2][12] =
{
	// opening material score
	82, 337, 365, 477, 1025, 12000, -82, -337, -365, -477, -1025, -12000,

	// endgame material score
	94, 281, 297, 512,  936, 12000, -94, -281, -297, -512,  -936, -12000
};

// game phase scores
const int opening_phase_score = 6192;
const int endgame_phase_score = 518;

// game phases
enum { opening, endgame, middlegame };

// piece types
enum { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

// positional piece scores [game phase][piece][square]
const int positional_score[2][6][64] =

// opening positional piece scores //
{
	//pawn
	0,   0,   0,   0,   0,   0,  0,   0,
	98, 134,  61,  95,  68, 126, 34, -11,
	-6,   7,  26,  31,  65,  56, 25, -20,
	-14,  13,   6,  21,  23,  12, 17, -23,
	-27,  -2,  -5,  12,  17,   6, 10, -25,
	-26,  -4,  -4, -10,   3,   3, 33, -12,
	-35,  -1, -20, -23, -15,  24, 38, -22,
	0,   0,   0,   0,   0,   0,  0,   0,

	// knight
	-167, -89, -34, -49,  61, -97, -15, -107,
	-73, -41,  72,  36,  23,  62,   7,  -17,
	-47,  60,  37,  65,  84, 129,  73,   44,
	-9,  17,  19,  53,  37,  69,  18,   22,
	-13,   4,  16,  13,  28,  19,  21,   -8,
	-23,  -9,  12,  10,  19,  17,  25,  -16,
	-29, -53, -12,  -3,  -1,  18, -14,  -19,
	-105, -21, -58, -33, -17, -28, -19,  -23,

	// bishop
	-29,   4, -82, -37, -25, -42,   7,  -8,
	-26,  16, -18, -13,  30,  59,  18, -47,
	-16,  37,  43,  40,  35,  50,  37,  -2,
	-4,   5,  19,  50,  37,  37,   7,  -2,
	-6,  13,  13,  26,  34,  12,  10,   4,
	0,  15,  15,  15,  14,  27,  18,  10,
	4,  15,  16,   0,   7,  21,  33,   1,
	-33,  -3, -14, -21, -13, -12, -39, -21,

	// rook
	32,  42,  32,  51, 63,  9,  31,  43,
	27,  32,  58,  62, 80, 67,  26,  44,
	-5,  19,  26,  36, 17, 45,  61,  16,
	-24, -11,   7,  26, 24, 35,  -8, -20,
	-36, -26, -12,  -1,  9, -7,   6, -23,
	-45, -25, -16, -17,  3,  0,  -5, -33,
	-44, -16, -20,  -9, -1, 11,  -6, -71,
	-19, -13,   1,  17, 16,  7, -37, -26,

	// queen
	-28,   0,  29,  12,  59,  44,  43,  45,
	-24, -39,  -5,   1, -16,  57,  28,  54,
	-13, -17,   7,   8,  29,  56,  47,  57,
	-27, -27, -16, -16,  -1,  17,  -2,   1,
	-9, -26,  -9, -10,  -2,  -4,   3,  -3,
	-14,   2, -11,  -2,  -5,   2,  14,   5,
	-35,  -8,  11,   2,   8,  15,  -3,   1,
	-1, -18,  -9,  10, -15, -25, -31, -50,

	// king
	-65,  23,  16, -15, -56, -34,   2,  13,
	29,  -1, -20,  -7,  -8,  -4, -38, -29,
	-9,  24,   2, -16, -20,   6,  22, -22,
	-17, -20, -12, -27, -30, -25, -14, -36,
	-49,  -1, -27, -39, -46, -44, -33, -51,
	-14, -14, -22, -46, -44, -30, -15, -27,
	1,   7,  -8, -64, -43, -16,   9,   8,
	-15,  36,  12, -54,   8, -28,  24,  14,


	// Endgame positional piece scores //

	//pawn
	0,   0,   0,   0,   0,   0,   0,   0,
	178, 173, 158, 134, 147, 132, 165, 187,
	94, 100,  85,  67,  56,  53,  82,  84,
	32,  24,  13,   5,  -2,   4,  17,  17,
	13,   9,  -3,  -7,  -7,  -8,   3,  -1,
	4,   7,  -6,   1,   0,  -5,  -1,  -8,
	13,   8,   8,  10,  13,   0,   2,  -7,
	0,   0,   0,   0,   0,   0,   0,   0,

	// knight
	-58, -38, -13, -28, -31, -27, -63, -99,
	-25,  -8, -25,  -2,  -9, -25, -24, -52,
	-24, -20,  10,   9,  -1,  -9, -19, -41,
	-17,   3,  22,  22,  22,  11,   8, -18,
	-18,  -6,  16,  25,  16,  17,   4, -18,
	-23,  -3,  -1,  15,  10,  -3, -20, -22,
	-42, -20, -10,  -5,  -2, -20, -23, -44,
	-29, -51, -23, -15, -22, -18, -50, -64,

	// bishop
	-14, -21, -11,  -8, -7,  -9, -17, -24,
	-8,  -4,   7, -12, -3, -13,  -4, -14,
	2,  -8,   0,  -1, -2,   6,   0,   4,
	-3,   9,  12,   9, 14,  10,   3,   2,
	-6,   3,  13,  19,  7,  10,  -3,  -9,
	-12,  -3,   8,  10, 13,   3,  -7, -15,
	-14, -18,  -7,  -1,  4,  -9, -15, -27,
	-23,  -9, -23,  -5, -9, -16,  -5, -17,

	// rook
	13, 10, 18, 15, 12,  12,   8,   5,
	11, 13, 13, 11, -3,   3,   8,   3,
	7,  7,  7,  5,  4,  -3,  -5,  -3,
	4,  3, 13,  1,  2,   1,  -1,   2,
	3,  5,  8,  4, -5,  -6,  -8, -11,
	-4,  0, -5, -1, -7, -12,  -8, -16,
	-6, -6,  0,  2, -9,  -9, -11,  -3,
	-9,  2,  3, -1, -5, -13,   4, -20,

	// queen
	-9,  22,  22,  27,  27,  19,  10,  20,
	-17,  20,  32,  41,  58,  25,  30,   0,
	-20,   6,   9,  49,  47,  35,  19,   9,
	3,  22,  24,  45,  57,  40,  57,  36,
	-18,  28,  19,  47,  31,  34,  39,  23,
	-16, -27,  15,   6,   9,  17,  10,   5,
	-22, -23, -30, -16, -16, -23, -36, -32,
	-33, -28, -22, -43,  -5, -32, -20, -41,

	// king
	-74, -35, -18, -18, -11,  15,   4, -17,
	-12,  17,  14,  17,  17,  38,  23,  11,
	10,  17,  23,  15,  20,  45,  44,  13,
	-8,  22,  24,  27,  26,  33,  26,   3,
	-18,  -4,  21,  24,  27,  23,   9, -11,
	-19,  -3,  11,  21,  23,  16,   7,  -9,
	-27, -11,   4,  13,  14,   4,  -5, -17,
	-53, -34, -21, -11, -28, -14, -24, -43
};






// mirror positional score tables for opposite side
const int mirror_score[128] =
{
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8
};




// double pawns penalty
const int double_pawn_penalty = -10;

// isolated pawn penalty
const int isolated_pawn_penalty = -10;

// passed pawn bonus
const int passed_pawn_bonus[8] = { 0, 5, 10, 20, 35, 60, 100, 200 };

// semi open file score
const int semi_open_file_score = 3;

// open file score
const int open_file_score = 5;

// king's shield bonus
const int king_shield_bonus = 5;


const int KingE[64] = {
	-50	,	-20	,	0	,	0	,	0	,	0	,	-20	,	-50	,
	-20,	0	,	20	,	20	,	20	,	20	,	0	,	-20	,
	0	,	20	,	40	,	40	,	40	,	40	,	20	,	0	,
	0	,	20	,	40	,	50	,	50	,	40	,	20	,	0	,
	0	,	20	,	40	,	50	,	50	,	40	,	20	,	0	,
	0	,	20	,	40	,	40	,	40	,	40	,	20	,	0	,
	-20	,	0	,	20	,	20	,	20	,	20	,	0	,	-20	,
	-50	,	-20	,	0	,	0	,	0	,	0	,	-20	,	-50
};

const int KingO[64] = {
	0	,	5	,	5	,	-10	,	-10	,	0	,	10	,	5	,
	-10	,	-10	,	-10	,	-10	,	-10	,	-10	,	-10	,	-10	,
	-30	,	-30	,	-30	,	-30	,	-30	,	-30	,	-30	,	-30	,
	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,
	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70	,	-70
};




// sjeng 11.2
int MaterialDraw(const S_Board* pos) {
	int white_rooks = count_bits(pos->bitboards[WR]);
	int black_rooks = count_bits(pos->bitboards[BR]);
	int white_bishops = count_bits(pos->bitboards[WB]);
	int black_bishops = count_bits(pos->bitboards[BB]);
	int white_knights = count_bits(pos->bitboards[WN]);
	int black_knights = count_bits(pos->bitboards[BN]);
	int white_queens = count_bits(pos->bitboards[WQ]);
	int black_queens = count_bits(pos->bitboards[BQ]);
	if (!white_rooks && !black_rooks && !white_queens && !black_queens) {
		if (!black_rooks && !white_bishops) {
			if (white_knights < 3 && black_knights < 3) {
				return TRUE;
			}
		}
		else if (!white_knights && !black_knights) {
			if (abs(white_bishops - black_bishops) < 2) {

				return TRUE;
			}
		}
		else if ((white_knights < 3 && !white_bishops) || (white_bishops == 1 && !white_knights)) {
			if ((black_knights < 3 && !black_bishops) || (black_bishops == 1 && !black_knights)) {

				return TRUE;
			}
		}
	}
	else if (!white_queens && !black_queens) {
		if (white_rooks == 1 && black_rooks == 1) {
			if ((white_knights + white_bishops) < 2 && (black_knights + black_bishops) < 2) {

				return TRUE;
			}
		}
		else if (white_rooks == 1 && !black_rooks) {
			if ((white_knights + white_bishops == 0) && (((black_knights + black_bishops) == 1) || ((black_knights + black_bishops) == 2))) { return TRUE; }
		}
		else if (black_rooks == 1 && !white_rooks) {
			if ((black_knights + black_bishops == 0) && (((white_knights + white_bishops) == 1) || ((white_knights + white_bishops) == 2))) { return TRUE; }
		}
	}
	return FALSE;
}

// get game phase score
static inline int get_game_phase_score(const S_Board* pos)
{
	/*
		The game phase score of the game is derived from the pieces
		(not counting pawns and kings) that are still on the board.
		The full material starting position game phase score is:

		4 * knight material score in the opening +
		4 * bishop material score in the opening +
		4 * rook material score in the opening +
		2 * queen material score in the opening
	*/

	// white & black game phase scores
	int white_piece_scores = 0, black_piece_scores = 0;

	// loop over white pieces
	for (int piece = WN; piece <= WQ; piece++)
		white_piece_scores += count_bits(pos->bitboards[piece]) * material_score[opening][piece];

	// loop over white pieces
	for (int piece = BN; piece <= BQ; piece++)
		black_piece_scores += count_bits(pos->bitboards[piece]) * -material_score[opening][piece];

	// return game phase score
	return white_piece_scores + black_piece_scores;
}

// position evaluation
int EvalPosition(const S_Board* pos)
{

	if (!count_bits(pos->bitboards[WP]) && !count_bits(pos->bitboards[BP]) && MaterialDraw(pos) == TRUE) {
		return 0;
	}


	// get game phase score
	int game_phase_score = get_game_phase_score(pos);

	// game phase (opening, middle game, endgame)
	int game_phase = -1;

	// pick up game phase based on game phase score
	if (game_phase_score > opening_phase_score) game_phase = opening;
	else if (game_phase_score < endgame_phase_score) game_phase = endgame;
	else game_phase = middlegame;


	// static evaluation score
	int score = 0, score_opening = 0, score_endgame = 0, double_pawns = 0;

	// current pieces bitboard copy
	Bitboard bitboard;

	// init piece & square
	int piece, square;

	// loop over piece pos->bitboards
	for (int bb_piece = WP; bb_piece <= BK; bb_piece++)
	{
		// init piece bitboard copy
		bitboard = pos->bitboards[bb_piece];

		// loop over pieces within a bitboard
		while (bitboard)
		{
			// init piece
			piece = bb_piece;

			// init square
			square = get_ls1b_index(bitboard);

			score_opening += material_score[opening][piece];
			score_endgame += material_score[endgame][piece];



			// score positional piece scores
			switch (piece)
			{
				// evaluate white pieces
			case WP:

				score_opening += positional_score[opening][PAWN][square];
				score_endgame += positional_score[endgame][PAWN][square];


				double_pawns = count_bits(pos->bitboards[WP] & FileBBMask[get_file[square]]);


				if (double_pawns > 1)
				{
					score += double_pawns * double_pawn_penalty;

				}


				// on isolated pawn
				if ((pos->bitboards[WP] & IsolatedMask[square]) == 0)
					// give an isolated pawn penalty
				{
					score += isolated_pawn_penalty;
				}



				// on passed pawn
				if ((WhitePassedMask[square] & pos->bitboards[BP]) == 0)
					// give passed pawn bonus
				{
					score += passed_pawn_bonus[get_rank[square]];
				}




				break;

			case WN:

				score_opening += positional_score[opening][KNIGHT][square];
				score_endgame += positional_score[endgame][KNIGHT][square];

				break;

			case WB:

				score_opening += positional_score[opening][BISHOP][square];
				score_endgame += positional_score[endgame][BISHOP][square];

				// mobility
				score += 5 * count_bits(get_bishop_attacks(square, pos->occupancies[BOTH]));



				break;

			case WR:
				/// interpolate scores in middle_game
				score_opening += positional_score[opening][ROOK][square];
				score_endgame += positional_score[endgame][ROOK][square];

				//open file
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & FileBBMask[get_file[square]]) == 0)
					// add  open file bonus
				{
					score += open_file_score;
				}

				// semi open file
				 if ((pos->bitboards[WP] & FileBBMask[get_file[square]]) == 0)
					// add semi open file bonus
				{
					score += semi_open_file_score;
				}

				// mobility
				score += 5 * count_bits(get_bishop_attacks(square, pos->occupancies[BOTH]));
				break;
			case WQ:

				score_opening += positional_score[opening][QUEEN][square];
				score_endgame += positional_score[endgame][QUEEN][square];

				//mobility
				score += 3 * count_bits(get_queen_attacks(square, pos->occupancies[BOTH]));

				break;
			case WK:

				score_opening += positional_score[opening][KING][square];
				score_endgame += positional_score[endgame][KING][square];

				
				//  open file
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & FileBBMask[get_file[square]]) == 0)
					// add semi open file penalty
				{
					score -= open_file_score;
				}



				// semi open file
				 if ((pos->bitboards[WP] & FileBBMask[get_file[square]]) == 0)
					// add semi open file penalty
				{
					score -= semi_open_file_score;
				}


				break;

				// evaluate black pieces
			case BP:

				score_opening -= positional_score[opening][PAWN][mirror_score[square]];
				score_endgame -= positional_score[endgame][PAWN][mirror_score[square]];

				// double pawn penalty
				double_pawns = count_bits(pos->bitboards[BP] & FileBBMask[get_file[square]]);

				// on double pawns (triple, etc)
				if (double_pawns > 1) {
					score -= double_pawns * double_pawn_penalty;
				}
				// on isolated pawnd
				if ((pos->bitboards[BP] & IsolatedMask[square]) == 0)
					// give an isolated pawn penalty
				{
					score -= isolated_pawn_penalty;
				}

				// on passed pawn
				if ((BlackPassedMask[square] & pos->bitboards[WP]) == 0)
					// give passed pawn bonus
				{
					score -= passed_pawn_bonus[7 - get_rank[square]];
				}


				break;


			case BN:
				score_opening -= positional_score[opening][KNIGHT][mirror_score[square]];
				score_endgame -= positional_score[endgame][KNIGHT][mirror_score[square]];

				break;

			case BB:
				// positional score
				score_opening -= positional_score[opening][BISHOP][mirror_score[square]];
				score_endgame -= positional_score[endgame][BISHOP][mirror_score[square]];

				// mobility
				score -= 5 * count_bits(get_bishop_attacks(square, pos->occupancies[BOTH]));


				break;
			case BR:

				score_opening -= positional_score[opening][ROOK][mirror_score[square]];
				score_endgame -= positional_score[endgame][ROOK][mirror_score[square]];

				// semi open file
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & FileBBMask[get_file[square]]) == 0)
					// add semi open file bonus
					score -= open_file_score;

				// semi open file
				 if ((pos->bitboards[BP] & FileBBMask[get_file[square]]) == 0)
					// add semi open file bonus
					score -= semi_open_file_score;


				// mobility
				score -= 5 * count_bits(get_bishop_attacks(square, pos->occupancies[BOTH]));

				break;
			case BQ:

				score_opening -= positional_score[opening][QUEEN][mirror_score[square]];
				score_endgame -= positional_score[endgame][QUEEN][mirror_score[square]];


				// mobility
				score -= 3 * count_bits(get_queen_attacks(square, pos->occupancies[BOTH]) & ~(pos->bitboards[pos->side*6+5] | pos->bitboards[pos->side * 6 + 4] ));
				break;

			case BK:
				score_opening -= positional_score[opening][KING][mirror_score[square]];
				score_endgame -= positional_score[endgame][KING][mirror_score[square]];
				
				// semi open file
				if ((pos->bitboards[BP] & FileBBMask[square]) == 0)
					// add semi open file penalty
					score += semi_open_file_score;

				// semi open file
				if (((pos->bitboards[WP] | pos->bitboards[BP]) & FileBBMask[get_file[square]]) == 0)
					// add semi open file penalty
					score += open_file_score;

				break;
			}


			// pop ls1b
			pop_bit(bitboard, square);
		}
	}


	if (game_phase == middlegame)
		score = (
			score_opening * game_phase_score +
			score_endgame * (opening_phase_score - game_phase_score)
			) / opening_phase_score;

	// return pure opening score in opening
	else if (game_phase == opening) score = score_opening;

	// return pure endgame score in engame
	else if (game_phase == endgame) score = score_endgame;


	// return final evaluation based on side
	return (pos->side == WHITE) ? score : -score;
}