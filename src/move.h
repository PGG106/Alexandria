#pragma once

struct S_Board;

struct S_MOVE {
    int move;
    int score;
};

// move list structure
struct S_MOVELIST {
    // moves
    S_MOVE moves[256];

    // move count
    int count;
};

enum class Movetype {
    Quiet, doublePush, KSCastle,
    QSCastle, Capture, enPassant,
    knightPromo, bishopPromo, rookPromo, queenPromo,
    knightCapturePromo = 12, bishopCapturePromo, rookCapturePromo, queenCapturePromo
};

int encode_move(const int source, const int target, const int piece, const int promoted, const Movetype movetype);
int From(const int move);
int To(const int move);
int Piece(const int move);
int Promoted(const int move);
bool IsCapture(const int move);
bool isEnpassant(const S_Board* pos, const int move);
bool IsQuiet(const int move);

