#include "Board.h"
#include "stdio.h"
#include "move.h"
#include "PieceData.h"
#include "movegen.h"
#include "makemove.h"
#include "io.h"
#include "perft.h"
#include "math.h"
#include "move.h"
#include <windows.h>



// get time in milliseconds
int get_time_ms()
{

    return GetTickCount64();

}

// leaf nodes (number of positions reached during the test of the move generator at a given depth)
unsigned long  long nodes;

// perft driver
static inline void perft_driver(int depth, S_Board* pos)
{



    // create move list instance
    S_MOVELIST move_list[1];

    // generate moves
    generate_moves(move_list, pos);



    // Bulk Counting
    if (depth == 1)
    {
        // increment nodes count (count reached positions)
        nodes += move_list->count;
        return;
    }



    // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {


        // make move
        make_move(move_list->moves[move_count].move, all_moves, pos);








        // call perft driver recursively
        perft_driver(depth - 1, pos);


        // take back


        Unmake_move(pos);


    }
}

// perft test
unsigned long  long perft_test(int depth, S_Board* pos)
{
    nodes = 0;
    printf("\n     Performance test\n\n");

    // create move list instance
    S_MOVELIST move_list[1];

    // generate moves
    generate_moves(move_list, pos);

    // init start time
    long start = get_time_ms();



    // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++)
    {//e3h3

        // make move
        make_move(move_list->moves[move_count].move, all_moves, pos);





        // cummulative nodes
        long cummulative_nodes = nodes;

        // call perft driver recursively
        perft_driver(depth - 1, pos);



        // take back

        Unmake_move(pos);


        // old nodes
        long old_nodes = nodes - cummulative_nodes;

        // print move
        printf("     move: %s%s%c  nodes: %ld\n", square_to_coordinates[get_move_source(move_list->moves[move_count].move)],
            square_to_coordinates[get_move_target(move_list->moves[move_count].move)],
            get_move_promoted(move_list->moves[move_count].move) ? promoted_pieces[get_move_promoted(move_list->moves[move_count].move)] : ' ',
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
