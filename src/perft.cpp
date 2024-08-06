#include "perft.h"
#include "position.h"
#include "piece_data.h"
#include "makemove.h"
#include "misc.h"
#include "movegen.h"
#include "move.h"
#include <iostream>

// leaf nodes (number of positions reached during the test of the move generator
// at a given depth)
uint64_t nodes;

// perft driver
void PerftDriver(int depth, Position* pos) {
    // create move list instance
    MoveList moveList;

    // Non bulk Counting
    if (depth == 0) {
        nodes += 1;
        return;
    }

    // generate moves
    GenerateMoves(&moveList, pos, MOVEGEN_ALL);

    if (depth == 1) {
        // loop over generated moves
        for (int moveCount = 0; moveCount < moveList.count; moveCount++) {
            Move move = moveList.moves[moveCount].move;
            if (!IsLegal(pos, move))
                continue;

            nodes += 1;
        }
        return;
    }

    // loop over generated moves
    for (int moveCount = 0; moveCount < moveList.count; moveCount++) {
        Move move = moveList.moves[moveCount].move;
        if (!IsLegal(pos, move))
            continue;

        // make move
        MakeMove<true>(move, pos);

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
    MoveList moveList;

    // generate moves
    GenerateMoves(&moveList, pos, MOVEGEN_ALL);

    // init start time
    long start = GetTimeMs();

    // loop over generated moves
    for (int moveCount = 0; moveCount < moveList.count; moveCount++) {
        const Move move = moveList.moves[moveCount].move;

        if (!IsLegal(pos, move))
            continue;

        // make move
        MakeMove<true>(move, pos);

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
