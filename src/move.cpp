#include "move.h"
#include "board.h"

int encode_move(const int source, const int target, const int piece, const int promoted, const Movetype movetype) {
    return (source) | (target << 6) | (piece << 12) | (promoted << 16) | (static_cast<int>(movetype) << 20);
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

int GetMovetype(const int move) {
    return ((move & 0xf00000) >> 20);
}

bool IsCapture(const int move) {
    return GetMovetype(move) & static_cast<int>(Movetype::Capture);
}

bool isEnpassant(const S_Board* pos, const int move) {
    return (PieceType[Piece(move)] == PAWN)  && (To(move) == GetEpSquare(pos));
}

bool IsQuiet(const int move) {
    return !IsCapture(move) && !Promoted(move);
}

bool IsCastle(const int move) {
    return (GetMovetype(move) == static_cast<int>(Movetype::KSCastle)) || (GetMovetype(move) == static_cast<int>(Movetype::QSCastle));
}
