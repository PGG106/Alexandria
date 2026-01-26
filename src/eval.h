#pragma once

#include "position.h"
#include <algorithm>
#include "misc.h"

[[nodiscard]] static inline int getMaterialValue(const Position* pos) {

    int pawns = CountBits(getPieceBB(pos, PAWN));
    int knights = CountBits(getPieceBB(pos, KNIGHT));
    int bishops = CountBits(getPieceBB(pos, BISHOP));
    int rooks = CountBits(getPieceBB(pos, ROOK));
    int queens = CountBits(getPieceBB(pos, QUEEN));

    return (pawns * 100 + knights * 422 + bishops * 422 + rooks * 642 + queens * 1015);
}

[[nodiscard]] static inline int ScaleMaterial(const Position* pos, int eval) {

    const int scale = 21000 + getMaterialValue(pos);
    return (eval * scale) / 27000;
}

[[nodiscard]] inline int EvalPositionRaw(Position* pos, NNUE::FinnyTable* FinnyPointer) {
    return NNUE::output(pos, FinnyPointer);
}

// position evaluation
[[nodiscard]] inline int EvalPosition(Position* pos, NNUE::FinnyTable* FinnyPointer) {
    int eval = EvalPositionRaw(pos, FinnyPointer);
    // Clamp eval to avoid it somehow being a mate score
    eval = std::clamp(eval, -MATE_FOUND + 1, MATE_FOUND - 1);
    return eval;
}

[[nodiscard]] inline int adjustEval(const Position *pos, const int correction, const int rawEval) {
    int adjustedEval = rawEval;

    adjustedEval = adjustedEval * (200 - pos->get50MrCounter()) / 200;

    adjustedEval = ScaleMaterial(pos, adjustedEval);

    adjustedEval += correction;

    return std::clamp(adjustedEval, -MATE_FOUND + 1, MATE_FOUND - 1);
}
