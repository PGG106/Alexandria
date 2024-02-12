#include "move.h"
#include "board.h"

int encode_move(const int source, const int target, const int piece, const Movetype movetype) {
    return (source) | (target << 6) | (static_cast<int>(movetype) << 12) | (piece << 16) ;
}

int From(const int move) {
    return move & 0x3f;
}

int To(const int move) {
    return ((move & 0xfc0) >> 6);
}

int GetMovetype(const int move) {
    return ((move & 0xf000) >> 12);
}

int Piece(const int move) {
    return ((move & 0xf0000) >> 16);
}

bool isEnpassant(const int move) {
    return GetMovetype(move) == static_cast<int>(Movetype::enPassant);
}

bool isDP(const int move) {
    return GetMovetype(move) == static_cast<int>(Movetype::doublePush);
}

bool isPromo(const int move) {
    return GetMovetype(move) & 8;
}

bool isCastle(const int move) {
    return (GetMovetype(move) == static_cast<int>(Movetype::KSCastle)) || (GetMovetype(move) == static_cast<int>(Movetype::QSCastle));
}

int getPromotedPiecetype(const int move) {
    return (GetMovetype(move) & 3) + 1;
}

bool isQuiet(const int move) {
    return !isCapture(move);
}

bool isCapture(const int move) {
    return GetMovetype(move) & static_cast<int>(Movetype::Capture);
}

// Shorthand for captures + any promotion no matter if quiet or not 
bool isTactical(const int move) {
    return isCapture(move) || isPromo(move);
}
