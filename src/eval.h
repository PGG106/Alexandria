#pragma once

#include "position.h"
#include <algorithm>

[[nodiscard]] static inline int getMaterialValue(const Position* pos) {

    int pawns = CountBits(GetPieceBB(pos, PAWN));
    int knights = CountBits(GetPieceBB(pos, KNIGHT));
    int bishops = CountBits(GetPieceBB(pos, BISHOP));
    int rooks = CountBits(GetPieceBB(pos, ROOK));
    int queens = CountBits(GetPieceBB(pos, QUEEN));

    return (pawns * 100 + knights * 422 + bishops * 422 + rooks * 642 + queens * 1015) / 32;
}

[[nodiscard]] static inline int ScaleMaterial(const Position* pos, int eval) {

    const int scale = 700 + getMaterialValue(pos);

    return (eval * scale) / 1024;
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
