#include "movepicker.h"

void pick_move(S_MOVELIST *move_list, int moveNum) {

  S_MOVE temp;
  int index = 0;
  int bestScore = 0;
  int bestNum = moveNum;

  for (index = moveNum; index < move_list->count; ++index) {

    if (move_list->moves[index].score > bestScore) {
      bestScore = move_list->moves[index].score;
      bestNum = index;
    }
  }

  temp = move_list->moves[moveNum];
  move_list->moves[moveNum] = move_list->moves[bestNum];
  move_list->moves[bestNum] = temp;
}