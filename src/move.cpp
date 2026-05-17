#include "move.h"

#include "position.h"

Move16 PackMove16(const Move move) {
    return static_cast<Move16>(move & 0xFFFF);
}

unsigned int Piece(const Position* pos, const Move16 move) {
    if (move == NOMOVE)
        return 0;
    return static_cast<unsigned int>(pos->PieceOn(From(move)));
}

unsigned int PieceTo(const Position* pos, const Move16 move) {
    return (Piece(pos, move) << 6) | To(move);
}

unsigned int PieceTypeTo(const Position* pos, const Move16 move) {
    return (PieceType[Piece(pos, move)] << 6) | To(move);
}

Move UnpackMove16(const Position* pos, const Move16 move16) {
    if (move16 == NOMOVE)
        return NOMOVE;
    return static_cast<Move>(move16) | (static_cast<Move>(Piece(pos, move16)) << 16);
}

unsigned int Piece(const Position* pos, const Move move) {
    return Piece(pos, PackMove16(move));
}

unsigned int PieceTo(const Position* pos, const Move move) {
    return PieceTo(pos, PackMove16(move));
}

unsigned int PieceTypeTo(const Position* pos, const Move move) {
    return PieceTypeTo(pos, PackMove16(move));
}
