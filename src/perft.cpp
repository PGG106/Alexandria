#include "perft.h"
#include "Board.h"
#include "PieceData.h"
#include "io.h"
#include "makemove.h"
#include "math.h"
#include "move.h"
#include "movegen.h"
#include "stdio.h"
#ifdef _WIN64
#include <windows.h>
#else
#include <sys/time.h>
#endif

// get time in milliseconds
int get_time_ms() {
#ifdef _WIN64
  return GetTickCount64();

#else
  struct timeval time_value;
  gettimeofday(&time_value, NULL);
  return time_value.tv_sec * 1000 + time_value.tv_usec / 1000;
#endif
}

// leaf nodes (number of positions reached during the test of the move generator
// at a given depth)
unsigned long long nodes;

// perft driver
void perft_driver(int depth, S_Board *pos) {

  // create move list instance
  S_MOVELIST move_list[1];

  // generate moves
  generate_moves(move_list, pos);

  // Bulk Counting
  if (depth == 1) {
    // increment nodes count (count reached positions)
    nodes += move_list->count;
    return;
  }

  // loop over generated moves
  for (int move_count = 0; move_count < move_list->count; move_count++) {

    // make move
    make_move(move_list->moves[move_count].move, pos);

    // call perft driver recursively
    perft_driver(depth - 1, pos);

    // take back

    Unmake_move(pos);
  }
}

// perft test
unsigned long long perft_test(int depth, S_Board *pos) {
  nodes = 0;
  printf("\n     Performance test\n\n");

  // create move list instance
  S_MOVELIST move_list[1];

  // generate moves
  generate_moves(move_list, pos);

  // init start time
  long start = get_time_ms();

  // loop over generated moves
  for (int move_count = 0; move_count < move_list->count; move_count++) { // e3h3

    // make move
    make_move(move_list->moves[move_count].move, pos);

    // cummulative nodes
    long cummulative_nodes = nodes;

    // call perft driver recursively
    perft_driver(depth - 1, pos);

    // take back

    Unmake_move(pos);

    // old nodes
    long old_nodes = nodes - cummulative_nodes;

    // print move
    printf("     move: %s%s%c  nodes: %ld\n",
           square_to_coordinates[get_move_source(
               move_list->moves[move_count].move)],
           square_to_coordinates[get_move_target(
               move_list->moves[move_count].move)],
           get_move_promoted(move_list->moves[move_count].move)
               ? promoted_pieces[get_move_promoted(
                     move_list->moves[move_count].move)]
               : ' ',
           old_nodes);
  }

  // print results
  printf("\n    Depth: %d\n", depth);
  printf("    Nodes: %llu\n", nodes);
  printf("     Time: %ld ms\n\n", get_time_ms() - start);
  unsigned long nodes_second = ((nodes / (ceil(get_time_ms() - start)))) * 1000;
  printf(" Nodes per second %lu\n\n", nodes_second);

  return nodes;
}
