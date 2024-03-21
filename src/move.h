#pragma once

struct Position;

struct S_MOVE {
    int move;
    int score;
};

// move list structure
struct MoveList {
    // moves
    S_MOVE moves[256];

    // move count
    int count;
};

enum class Movetype {
    Quiet, doublePush, KSCastle,
    QSCastle, Capture, enPassant,
    knightPromo=8, bishopPromo, rookPromo, queenPromo,
    knightCapturePromo, bishopCapturePromo, rookCapturePromo, queenCapturePromo
};

inline Movetype operator | (Movetype first, Movetype second){
    Movetype cong = static_cast<Movetype>((static_cast<int>(first) | static_cast<int>(second)));
    return cong;
}

int encode_move(const int source, const int target, const int piece, const Movetype movetype);
int From(const int move);
int To(const int move);
int Piece(const int move);
int getPromotedPiecetype(const int move);
int GetMovetype(const int move);
bool isEnpassant(const int move);
bool isDP(const int move);
bool isQuiet(const int move);
bool isCapture(const int move);
bool isTactical(const int move);
bool isPromo(const int move);
bool isCastle(const int move);
