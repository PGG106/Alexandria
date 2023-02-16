#include "perft.h"
#include "pieceData.h"
#include "io.h"
#include "makemove.h"
#include "misc.h"
#ifdef _WIN64
#include <windows.h>
#else
#include <sys/time.h>
#endif



// leaf nodes (number of positions reached during the test of the move generator
// at a given depth)
unsigned long long nodes;

// perft driver
void PerftDriver(int depth, S_Board* pos) {
	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	GenerateMoves(move_list, pos);

	// Bulk Counting
	if (depth == 1) {
		// increment nodes count (count reached positions)
		nodes += move_list->count;
		return;
	}

	// Bulk Counting
	if (depth == 0) {
		nodes += 1;
		return;
	}

	// loop over generated moves
	for (int move_count = 0; move_count < move_list->count; move_count++) {
		int move = move_list->moves[move_count].move;
		// make move
		make_move(move, pos);

		// call perft driver recursively
		PerftDriver(depth - 1, pos);

		// take back

		Unmake_move(move, pos);
	}
}

// perft test
unsigned long long PerftTest(int depth, S_Board* pos) {
	nodes = 0;
	printf("\n     Performance test\n\n");

	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	GenerateMoves(move_list, pos);

	// init start time
	long start = GetTimeMs();

	// loop over generated moves
	for (int move_count = 0; move_count < move_list->count; move_count++) {
		const int move = move_list->moves[move_count].move;
		// make move
		make_move(move, pos);

		// cummulative nodes
		long cummulative_nodes = nodes;

		// call perft driver recursively
		PerftDriver(depth - 1, pos);

		// take back

		Unmake_move(move, pos);

		// old nodes
		long old_nodes = nodes - cummulative_nodes;

		// print move
		printf(" %s%s%c: %ld\n",
			square_to_coordinates[From(move)],
			square_to_coordinates[To(move)],
			get_move_promoted(move)
			? promoted_pieces[get_move_promoted(move)]
			: ' ',
			old_nodes);
	}

	auto time = GetTimeMs() - start;
	// print results
	printf("\n    Depth: %d\n", depth);
	printf("    Nodes: %llu\n", nodes);
	printf("     Time: %ld ms\n\n", time);
	unsigned long nodes_second = (nodes / time + !time) * 1000;
	printf(" Nodes per second %lu\n\n", nodes_second);

	return nodes;
}
