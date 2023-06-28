#pragma once

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
#define Piece(move) ((move & 0xf000) >> 12)
// extract promoted piece
#define Promoted(move) ((move & 0xf0000) >> 16)
// extract capture flag
#define IsCapture(move) (move & 0x100000)
#define isEnpassant(pos,move) ((Piece(move) == WP || Piece(move) == BP) && (To(move) == GetEpSquare(pos)))

#define IsQuiet(move) (!IsCapture(move) && !Promoted(move))
