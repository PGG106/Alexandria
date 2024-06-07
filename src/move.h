#pragma once

#include "types.h"

struct Position;

struct ScoredMove {
    int move;
    int score;
};

// move list structure
struct MoveList {
    ScoredMove moves[256];
    int count = 0;
};

enum class Movetype {
    Quiet, doublePush, KSCastle,
    QSCastle, Capture, enPassant,
    knightPromo=8, bishopPromo, rookPromo, queenPromo,
    knightCapturePromo, bishopCapturePromo, rookCapturePromo, queenCapturePromo
};

inline Movetype operator | (Movetype first, Movetype second){
    return static_cast<Movetype>((static_cast<int>(first) | static_cast<int>(second)));
}

Move encode_move(const int source, const int target, const int piece, const Movetype movetype);
int From(const Move move);
int To(const Move move);
int FromTo(const Move move);
int Piece(const Move move);
int PieceTo(const Move move);
int getPromotedPiecetype(const Move move);
int GetMovetype(const Move move);
bool isEnpassant(const Move move);
bool isDP(const Move move);
bool isQuiet(const Move move);
bool isCapture(const Move move);
bool isTactical(const Move move);
bool isPromo(const Move move);
bool isCastle(const Move move);
