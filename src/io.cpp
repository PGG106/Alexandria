// print bitboard
#include "Board.h"
#include "PieceData.h"
#include "attack.h"
#include "move.h"
#include "movegen.h"
#include "stdio.h"

#define FR2SQ(rank, file) (64 - ((file << 3) | rank))

void print_bitboard(Bitboard bitboard) {
  // print offset
  printf("\n");

  // loop over board ranks
  for (int rank = 0; rank < 8; rank++) {
    // loop over board files
    for (int file = 0; file < 8; file++) {
      // convert file & rank into square index
      int square = (rank * 8 + file);

      // print ranks
      if (!file)
        printf("  %d ", 8 - rank);

      // print bit state (either 1 or 0)
      printf(" %d", get_bit(bitboard, square) ? 1 : 0);
    }

    // print new line every rank
    printf("\n");
  }

  // print board files
  printf("\n     a b c d e f g h\n\n");

  // print bitboard as unsigned decimal number
  printf("     Bitboard: %llud\n\n", bitboard);
}

// print board
void print_board(S_Board *pos) {
  // print offset
  printf("\n");

  // loop over board ranks
  for (int rank = 0; rank < 8; rank++) {
    // loop ober board files
    for (int file = 0; file < 8; file++) {
      // init square
      int square = rank * 8 + file;

      // print ranks
      if (!file)
        printf("  %d ", 8 - rank);

      // define piece variable
      int piece = -1;

      // loop over all piece pos->bitboards
      for (int bb_piece = WP; bb_piece <= BK; bb_piece++) {
        // if there is a piece on current square
        if (get_bit(pos->bitboards[bb_piece], square))
          // get piece code
          piece = bb_piece;
      }

      printf(" %c", (piece == -1) ? '.' : ascii_pieces[piece]);
    }

    // print new line every rank
    printf("\n");
  }

  // print board files
  printf("\n     a b c d e f g h\n\n");

  // print side to move
  printf("     Side:     %s\n", !pos->side ? "white" : "black");

  // print enpassant square
  printf("     Enpassant:   %s\n",
         (pos->enPas != no_sq) ? square_to_coordinates[pos->enPas] : "no");

  // print castling rights
  printf("     Castling:  %c%c%c%c\n", (pos->castleperm & WKCA) ? 'K' : '-',
         (pos->castleperm & WQCA) ? 'Q' : '-',
         (pos->castleperm & BKCA) ? 'k' : '-',
         (pos->castleperm & BQCA) ? 'q' : '-');

  printf("     position ply: %d\n", pos->ply);

  printf("     position hisPly: %d\n", pos->hisPly);

  printf("     position key: %llx\n\n", pos->posKey);

}

// print attacked squares
void print_attacked_squares(S_Board *pos, int side) {
  printf("\n");

  // loop over board ranks
  for (int rank = 0; rank < 8; rank++) {
    // loop over board files
    for (int file = 0; file < 8; file++) {
      // init square
      int square = rank * 8 + file;

      // print ranks
      if (!file)
        printf("  %d ", 8 - rank);

      // check whether current square is attacked or not
      printf(" %d", is_square_attacked(pos, square, side) ? 1 : 0);
    }

    // print new line every rank
    printf("\n");
  }

  // print files
  printf("\n     a b c d e f g h\n\n");
}

// print move (for UCI purposes)
void print_move(const int move) {
  const char *from = square_to_coordinates[From(move)];
  const char *to = square_to_coordinates[To(move)];
  char promoted = get_move_promoted(move);

  if (promoted)
    printf("%s%s%c ", from, to, promoted_pieces[promoted]);
  else
    printf("%s%s ", from, to);
}

char *FormatMove(const int move) {
  static char Move[6];
  const char *from = square_to_coordinates[From(move)];
  const char *to = square_to_coordinates[To(move)];
  char promoted = get_move_promoted(move);

  if (promoted)
    snprintf(Move, sizeof(Move), "%s%s%c", from, to, promoted_pieces[promoted]);
  else
    snprintf(Move, sizeof(Move), "%s%s", from, to);

  return Move;
}

void PrintMoveList(S_MOVELIST *list) {
  int index = 0;
  int score = 0;
  int move = 0;
  printf("MoveList:%d\n", list->count);
  for (index = 0; index < list->count; ++index) {
    move = list->moves[index].move;
    score = list->moves[index].score;
    printf("Move:%d > %s (score:%d) \n", index + 1, FormatMove(move), score);
  }
  printf("MoveList Total  Moves:%d\n\n", list->count);
}
