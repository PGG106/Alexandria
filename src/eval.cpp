#include "eval.h"
#include "position.h"
#include <algorithm>

// if we don't have enough material to mate consider the position a draw
bool MaterialDraw(const Position* pos) {
    // If we only have kings on the board then it's a draw
    if (pos->PieceCount() == 2)
        return true;
    // KN v K, KB v K
    else if (pos->PieceCount() == 3 && ((CountBits(GetPieceBB(pos, KNIGHT)) == 1) || (CountBits(GetPieceBB(pos, BISHOP)) == 1)))
        return true;
    // If we have 4 pieces on the board
    else if (pos->PieceCount() == 4) {
        // KNN v K, KN v KN
        if ((CountBits(GetPieceBB(pos, KNIGHT)) == 2))
            return true;
        // KB v KB
        else if (((CountBits(GetPieceBB(pos, BISHOP)) == 2)) && CountBits(pos->GetPieceColorBB(BISHOP, WHITE)) == 1)
            return true;
    }

    return false;
}

// get game phase score
static inline int get_game_phase_score(const Position* pos)
{
    // white & black game phase scores
    int white_piece_scores = 0, black_piece_scores = 0;

    // loop over white pieces
    for (int piece = WN; piece <= WQ; piece++)
        white_piece_scores += CountBits(pos->bitboards[piece]) * material_score[opening][piece];

    // loop over white pieces
    for (int piece = BN; piece <= BQ; piece++)
        black_piece_scores += CountBits(pos->bitboards[piece]) * -material_score[opening][piece];

    // return game phase score
    return white_piece_scores + black_piece_scores;
}

// position evaluation
int EvalPosition(Position* pos)
{

    // get game phase score
    int game_phase_score = get_game_phase_score(pos);

    // game phase (opening, middle game, endgame)
    int game_phase = -1;

    // pick up game phase based on game phase score
    if (game_phase_score > opening_phase_score) game_phase = opening;
    else if (game_phase_score < endgame_phase_score) game_phase = endgame;
    else game_phase = middlegame;

    // static evaluation score
    int score = 0, score_opening = 0, score_endgame = 0;


    // init piece & square
    int square;

    Bitboard white_pawns = pos->bitboards[WP];

    while (white_pawns) {
        // init square
        square = popLsb(white_pawns);

        score_opening += material_score[opening][WP];
        score_endgame += material_score[endgame][WP];

        score_opening += positional_score[opening][PAWN][square];
        score_endgame += positional_score[endgame][PAWN][square];
    }

    Bitboard white_knights = pos->bitboards[WN];

    while (white_knights) {
        // init square
        square = popLsb(white_knights);

        score_opening += material_score[opening][WN];
        score_endgame += material_score[endgame][WN];

        score_opening += positional_score[opening][KNIGHT][square];
        score_endgame += positional_score[endgame][KNIGHT][square];
    }

    Bitboard white_bishops = pos->bitboards[WB];

    while (white_bishops) {
        // init square
        square = popLsb(white_bishops);

        score_opening += material_score[opening][WB];
        score_endgame += material_score[endgame][WB];

        score_opening += positional_score[opening][BISHOP][square];
        score_endgame += positional_score[endgame][BISHOP][square];
    }

    Bitboard white_rooks = pos->bitboards[WR];

    while (white_rooks) {
        // init square
        square = popLsb(white_rooks);

        score_opening += material_score[opening][WR];
        score_endgame += material_score[endgame][WR];

        score_opening += positional_score[opening][ROOK][square];
        score_endgame += positional_score[endgame][ROOK][square];
    }

    Bitboard white_queens = pos->bitboards[WQ];

    while (white_queens) {
        // init square
        square = popLsb(white_queens);

        score_opening += material_score[opening][WQ];
        score_endgame += material_score[endgame][WQ];

        score_opening += positional_score[opening][QUEEN][square];
        score_endgame += positional_score[endgame][QUEEN][square];

    }

    int kingsquare = GetLsbIndex(pos->bitboards[WK]);

    score_opening += material_score[opening][WK];
    score_endgame += material_score[endgame][WK];

    score_opening += positional_score[opening][KING][kingsquare];
    score_endgame += positional_score[endgame][KING][kingsquare];

    Bitboard black_pawns = pos->bitboards[BP];
    while (black_pawns) {

        // init square
        square = popLsb(black_pawns);

        score_opening += material_score[opening][BP];
        score_endgame += material_score[endgame][BP];

        score_opening -= positional_score[opening][PAWN][square^56];
        score_endgame -= positional_score[endgame][PAWN][square^56];

    }

    Bitboard black_knights = pos->bitboards[BN];
    while (black_knights) {

        // init square
        square = popLsb(black_knights);


        score_opening += material_score[opening][BN];
        score_endgame += material_score[endgame][BN];

        score_opening -= positional_score[opening][KNIGHT][square^56];
        score_endgame -= positional_score[endgame][KNIGHT][square^56];

    }

    Bitboard black_bishops = pos->bitboards[BB];
    while (black_bishops) {

        // init square
        square = popLsb(black_bishops);

        score_opening += material_score[opening][BB];
        score_endgame += material_score[endgame][BB];

        score_opening -= positional_score[opening][BISHOP][square^56];
        score_endgame -= positional_score[endgame][BISHOP][square^56];

    }

    Bitboard black_rooks = pos->bitboards[BR];
    while (black_rooks) {

        // init square
        square = popLsb(black_rooks);

        score_opening += material_score[opening][BR];
        score_endgame += material_score[endgame][BR];

        score_opening -= positional_score[opening][ROOK][square^56];
        score_endgame -= positional_score[endgame][ROOK][square^56];

    }

    Bitboard black_queens = pos->bitboards[BQ];

    while (black_queens) {

        // init square
        square = popLsb(black_queens);

        score_opening += material_score[opening][BQ];
        score_endgame += material_score[endgame][BQ];

        score_opening -= positional_score[opening][QUEEN][square^56];
        score_endgame -= positional_score[endgame][QUEEN][square^56];

    }

    kingsquare = GetLsbIndex(pos->bitboards[BK]);

    score_opening += material_score[opening][BK];
    score_endgame += material_score[endgame][BK];

    score_opening -= positional_score[opening][KING][kingsquare^56];
    score_endgame -= positional_score[endgame][KING][kingsquare^56];

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
