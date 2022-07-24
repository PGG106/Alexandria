#include "Board.h"
#include "move.h"

void updateHH(S_Board *pos, int depth, int bestmove, S_MOVELIST *quiet_moves) {
  if (depth > 1)
    pos->searchHistory[pos->pieces[get_move_source(bestmove)]]
                      [get_move_target(bestmove)] += 1 << depth;
  for (int i = 0; i < quiet_moves->count; i++) {
    int move = quiet_moves->moves[i].move;
    if (move == bestmove)
      continue;
    else {
      pos->searchHistory[pos->pieces[get_move_source(move)]]
                        [get_move_target(move)] -= 1 << depth;
    }
  }
}