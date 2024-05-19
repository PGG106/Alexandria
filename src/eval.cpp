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

// position evaluation
int materialScore(Position* pos) {
    int white_pawns = SEEValue[PAWN] * CountBits(pos->GetPieceColorBB(PAWN,WHITE));
    int white_knights = SEEValue[KNIGHT] * CountBits(pos->GetPieceColorBB(KNIGHT,WHITE));
    int white_bishops = SEEValue[BISHOP] * CountBits(pos->GetPieceColorBB(BISHOP,WHITE));
    int white_rooks = SEEValue[ROOK] * CountBits(pos->GetPieceColorBB(ROOK,WHITE));
    int white_queens = SEEValue[QUEEN] * CountBits(pos->GetPieceColorBB(QUEEN,WHITE));

    int white_score = white_pawns + white_knights + white_bishops + white_rooks + white_queens;

    int black_pawns = SEEValue[PAWN] * CountBits(pos->GetPieceColorBB(PAWN,BLACK));
    int black_knights = SEEValue[KNIGHT] * CountBits(pos->GetPieceColorBB(KNIGHT,BLACK));
    int black_bishops = SEEValue[BISHOP] * CountBits(pos->GetPieceColorBB(BISHOP,BLACK));
    int black_rooks = SEEValue[ROOK] * CountBits(pos->GetPieceColorBB(ROOK,BLACK));
    int black_queens = SEEValue[QUEEN] * CountBits(pos->GetPieceColorBB(QUEEN,BLACK));

    int black_score = black_pawns + black_knights + black_bishops + black_rooks + black_queens;


    return white_score - black_score;
}



// position evaluation
int EvalPosition(Position* pos) {
    int score = pos->side == WHITE  ?  materialScore(pos) : -materialScore(pos);
    score += (pos->GetPoskey() % 4);
    return score;
}
