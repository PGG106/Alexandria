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

[[nodiscard]] static inline int ScaleMaterial(const Position* pos, int eval) {
    const int knights = CountBits(GetPieceBB(pos, KNIGHT));
    const int bishops = CountBits(GetPieceBB(pos, BISHOP));
    const int rooks = CountBits(GetPieceBB(pos, ROOK));
    const int queens = CountBits(GetPieceBB(pos, QUEEN));
    const int phase = std::min(3 * knights + 3 * bishops + 5 * rooks + 10 * queens, 64);
    // Scale between [0.75, 1.00]
    return eval * (192 + phase) / 256;
}

[[nodiscard]] inline int EvalPositionRaw(Position* pos) {
    return nnue.output(pos);
}

// position evaluation
[[nodiscard]] inline int EvalPosition(Position* pos) {
    int eval = EvalPositionRaw(pos);
    eval = ScaleMaterial(pos, eval);
    eval = eval * (200 - pos->get50MrCounter()) / 200;
    eval = (eval / 16) * 16 - 1 + (pos->posKey & 0x2);
    // Clamp eval to avoid it somehow being a mate score
    eval = std::clamp(eval, -MATE_FOUND + 1, MATE_FOUND - 1);
    return eval;
}
