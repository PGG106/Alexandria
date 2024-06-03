#include "move.h"

int encode_move(const int source, const int target, const int piece, const Movetype movetype) {
    return (source) | (target << 6) | (static_cast<int>(movetype) << 12) | (piece << 16);
}

int From(const Move move) {
    return move & 0x3F;
}

int To(const Move move) {
    return ((move & 0xFC0) >> 6);
}

int FromTo(const Move move) {
    return move & 0xFFF;
}

int Piece(const Move move) {
    return ((move & 0xF0000) >> 16);
}

int PieceTo(const Move move) {
    return (Piece(move) << 6) | To(move);
}

int GetMovetype(const Move move) {
    return ((move & 0xF000) >> 12);
}

bool isEnpassant(const Move move) {
    return GetMovetype(move) == static_cast<int>(Movetype::enPassant);
}

bool isDP(const Move move) {
    return GetMovetype(move) == static_cast<int>(Movetype::doublePush);
}

bool isPromo(const Move move) {
    return GetMovetype(move) & 8;
}

bool isCastle(const Move move) {
    return (GetMovetype(move) == static_cast<int>(Movetype::KSCastle)) || (GetMovetype(move) == static_cast<int>(Movetype::QSCastle));
}

int getPromotedPiecetype(const Move move) {
    return (GetMovetype(move) & 3) + 1;
}

bool isQuiet(const Move move) {
    return !isCapture(move);
}

bool isCapture(const Move move) {
    return GetMovetype(move) & static_cast<int>(Movetype::Capture);
}

// Shorthand for captures + any promotion no matter if quiet or not 
bool isTactical(const Move move) {
    return isCapture(move) || isPromo(move);
}
