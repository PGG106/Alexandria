#pragma once

typedef struct S_MOVE {
    int move;
    int score;
}S_MOVE;

// move list structure
typedef struct {
    // moves
    S_MOVE moves[256];

    // move count
    int count;
} S_MOVELIST;

// encode move
#define encode_move(source, target, piece, promoted, capture, double_push, enpassant, castling) \
    (source) |          \
    (target << 6) |     \
    (piece << 12) |     \
    (promoted << 16) |  \
    (capture << 20) |   \
    (double_push << 21) |    \
    (enpassant << 22) | \
    (castling << 23)  

#define NOMOVE 0
#define MAXSCORE 32670
#define mate_value 32000
#define mate_score 31000
#define ISMATE (mate_value - MAXDEPTH)

// extract source square
#define get_move_source(move) (move & 0x3f)
// extract target square
#define get_move_target(move) ((move & 0xfc0) >> 6)
// extract piece
#define get_move_piece(move) ((move & 0xf000) >> 12)
// extract promoted piece
#define get_move_promoted(move) ((move & 0xf0000) >> 16)
// extract capture flag
#define get_move_capture(move) (move & 0x100000)
// extract double pawn push flag
#define get_move_double(move) (move & 0x200000)
// extract enpassant flag
#define get_move_enpassant(move) (move & 0x400000)
// extract castling flag
#define get_move_castling(move) (move & 0x800000)



// move types
enum { all_moves, only_captures };

