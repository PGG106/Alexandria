#include "move.h"

#include "position.h"

unsigned int Piece(const Position* pos, const Move move) {
    if (move == NOMOVE)
        return 0;

    return pos->PieceOn(From(move));
}

unsigned int PieceTo(const Position* pos, const Move move) {
    return (Piece(pos, move) << 6) | To(move);
}

unsigned int PieceTypeTo(const Position* pos, const Move move) {
    return (GetPieceType(Piece(pos, move)) << 6) | To(move);
}
