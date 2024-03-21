#include "perft.h"
#include "position.h"
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
void PerftDriver(int depth, Position* pos) {
    // create move list instance
    S_MOVELIST moveList;

    // Non bulk Counting
    if (depth == 0) {
        nodes += 1;
        return;
    }

    // generate moves
    GenerateMoves(&moveList, pos);

    // loop over generated moves
    for (int moveCount = 0; moveCount < moveList.count; moveCount++) {
        int move = moveList.moves[moveCount].move;
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
unsigned long long PerftTest(int depth, Position* pos) {
    nodes = 0;
    std::cout << ("\n     Performance test\n\n");

    // create move list instance
    S_MOVELIST moveList;

    // generate moves
    GenerateMoves(&moveList, pos);

    // init start time
    long start = GetTimeMs();

    // loop over generated moves
    for (int moveCount = 0; moveCount < moveList.count; moveCount++) {
        const int move = moveList.moves[moveCount].move;

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
