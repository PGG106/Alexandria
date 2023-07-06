#include "move.h"
#include "board.h"

int encode_move(const int source, const int target, const int piece, const int promoted, const int capture) {
    return (source) | (target << 6) | (piece << 12) | (promoted << 16) | (capture << 20);
}

int From(const int move) {
    return move & 0x3f;
}

int To(const int move) {
    return ((move & 0xfc0) >> 6);
}

int Piece(const int move) {
    return ((move & 0xf000) >> 12);
}

int Promoted(const int move) {
    return ((move & 0xf0000) >> 16);
}

bool IsCapture(const int move) {
    return (move & 0x100000);
}

bool isEnpassant(const S_Board* pos, const int move) {
    return (PieceType[Piece(move)] == PAWN)  && (To(move) == GetEpSquare(pos));
}

bool IsQuiet(const int move) {
    return !IsCapture(move) && !Promoted(move);
}
