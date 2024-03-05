#include "perft.h"
#include "board.h"
#include "piece_data.h"
#include "io.h"
#include "makemove.h"
#include "misc.h"
#include "movegen.h"
#include "move.h"
#include <iostream>

// leaf nodes (number of positions reached during the test of the move generator
// at a given depth)
unsigned long long nodes;

// perft driver
void PerftDriver(int depth, S_Board* pos) {
    // create move list instance
    S_MOVELIST move_list[1];

    // Non bulk Counting
    if (depth == 0) {
        nodes += 1;
        return;
    }

    // generate moves
    GenerateMoves(move_list, pos);

    // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++) {
        int move = move_list->moves[move_count].move;
        if (!IsLegal(pos, move))
            continue;

        // make move
        MakeMove(move, pos);

        // call perft driver recursively
        PerftDriver(depth - 1, pos);

        // take back
        UnmakeMove(move, pos);
    }
}

// perft test
unsigned long long PerftTest(int depth, S_Board* pos) {
    nodes = 0;
    std::cout << ("\n     Performance test\n\n");

    // create move list instance
    S_MOVELIST move_list[1];

    // generate moves
    GenerateMoves(move_list, pos);

    // init start time
    long start = GetTimeMs();

    // loop over generated moves
    for (int move_count = 0; move_count < move_list->count; move_count++) {
        const int move = move_list->moves[move_count].move;

        if (!IsLegal(pos, move))
            continue;

        // make move
        MakeMove(move, pos);

        // cummulative nodes
        long cummulative_nodes = nodes;

        // call perft driver recursively
        PerftDriver(depth - 1, pos);

        // take back
        UnmakeMove(move, pos);

        // old nodes
        long old_nodes = nodes - cummulative_nodes;

        // print move
        printf(" %s%s%c: %ld\n",
            square_to_coordinates[From(move)],
            square_to_coordinates[To(move)],
            isPromo(move)
            ? promoted_pieces[getPromotedPiecetype(move)]
            : ' ',
            old_nodes);
    }

    auto time = GetTimeMs() - start;
    // print results
    std::cout << "\n    Depth: " << depth << "\n";
    std::cout << "    Nodes: " << nodes << "\n";
    std::cout << "     Time: " << time << "\n";
    unsigned long nodes_second = (nodes / (time + !time)) * 1000;
    std::cout << " Nodes per second: " << nodes_second << "\n\n";

    return nodes;
}
