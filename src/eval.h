#pragma once

#include "position.h"
#include <algorithm>

// if we don't have enough material to mate consider the position a draw
[[nodiscard]] inline bool MaterialDraw(const Position* pos) {
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

[[nodiscard]] static inline int getMaterialValue(const Position* pos) {

    int pawns = CountBits(GetPieceBB(pos, PAWN));
    int knights = CountBits(GetPieceBB(pos, KNIGHT));
    int bishops = CountBits(GetPieceBB(pos, BISHOP));
    int rooks = CountBits(GetPieceBB(pos, ROOK));
    int queens = CountBits(GetPieceBB(pos, QUEEN));

    return (pawns * 100 + knights * 422 + bishops * 422 + rooks * 642 + queens * 1015) / 32;
}

[[nodiscard]] static inline int ScaleMaterial(const Position* pos, int eval) {

    return eval;
}

[[nodiscard]] inline int EvalPositionRaw(Position* pos, NNUE::FinnyTable* FinnyPointer) {
    return NNUE::output(pos, FinnyPointer);
}

// position evaluation
[[nodiscard]] inline int EvalPosition(Position* pos, NNUE::FinnyTable* FinnyPointer) {
    int eval = EvalPositionRaw(pos, FinnyPointer);
    eval = ScaleMaterial(pos, eval);
    eval = eval * (200 - pos->get50MrCounter()) / 200;
    // Clamp eval to avoid it somehow being a mate score
    eval = std::clamp(eval, -MATE_FOUND + 1, MATE_FOUND - 1);
    return eval;
}
