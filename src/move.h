#pragma once

typedef struct S_MOVE {
	int move;
	int score;
} S_MOVE;

// move list structure
typedef struct {
	// moves
	S_MOVE moves[256];

	// move count
	int count;
} S_MOVELIST;

// encode move
#define encode_move(source, target, piece, promoted, capture)      \
  (source) | (target << 6) | (piece << 12) | (promoted << 16) |                \
      (capture << 20)         

#define NOMOVE 0
#define mate_score 31000
#define mate_value 32000
#define value_none 32001
#define MAXSCORE 32670

#define ISMATE (mate_value - MAXDEPTH)

// extract source square
#define From(move) (move & 0x3f)
// extract target square
#define To(move) ((move & 0xfc0) >> 6)
// extract piece
#define get_move_piece(move) ((move & 0xf000) >> 12)
// extract promoted piece
#define get_move_promoted(move) ((move & 0xf0000) >> 16)
// extract capture flag
#define get_move_capture(move) (move & 0x100000)


#define IsQuiet(move) (!get_move_capture(move) && !get_move_promoted(move))

#define isEnpassant(pos,move) ((get_move_piece(move) == WP || get_move_piece(move) == BP) && (To(move) == pos->enPas))

// move types
enum { all_moves, only_captures };

