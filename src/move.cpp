#include "move.h"

#include "position.h"

unsigned int Piece(const Position* pos, const Move move) {
    if (move == NOMOVE)
        return 0;

    const int piece = pos->PieceOn(From(move));
    // Keep piece-derived indices in range even for stale TT/killer/counter moves.
    return static_cast<unsigned int>((piece >= WP && piece <= BK) ? piece : WP);
}

unsigned int PieceTo(const Position* pos, const Move move) {
    return (Piece(pos, move) << 6) | To(move);
}

unsigned int PieceTypeTo(const Position* pos, const Move move) {
    return (GetPieceType(Piece(pos, move)) << 6) | To(move);
}
