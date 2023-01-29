#include "Board.h"
#include "movegen.h"
#include <time.h>
#include <stdlib.h>
#include "makemove.h"
#include "datagen.h"
#include <iostream>
#include <fstream>
#include "misc.h"

int random_moves = 0;
bool do_datagen = false;
enum line_type {
	result,
	fen,
	moves

};
void make_random_move(S_Board* pos) {

	S_MOVELIST move_list[1];

	// generate moves
	generate_moves(move_list, pos);

	int r = rand() % move_list->count;
	int random_move = move_list->moves[r].move;
	printf("bestmove ");
	print_move(random_move);
	printf("\n");
	return;
}

int search_best_move(S_ThreadData* td, Search_stack* ss)
{
	S_Board* pos = &td->pos;
	Search_data* sd = &td->ss;
	S_SearchINFO* info = &td->info;

	//variable used to store the score of the best move found by the search (while the move itself can be retrieved from the TT)
	int score = 0;

	//Clean the position and the search info to start search from a clean state 
	ClearForSearch(td);

	// define initial alpha beta bounds
	int alpha = -MAXSCORE;
	int beta = MAXSCORE;
	// Call the negamax function in an iterative deepening framework
	for (int current_depth = 1; current_depth <= info->depth; current_depth++)
	{
		score = negamax(alpha, beta, current_depth, td, ss);

		// check if we just cleared a depth and more than OptTime passed
		if (info->timeset && GetTimeMs() > info->stoptimeOpt)
			info->stopped = true;

		if (info->stopped)
			// stop calculating and return best move so far
			break;

	}
	return score;
}


void datagen(S_ThreadData* td, Search_stack* ss)
{
	S_Board* pos = &td->pos;
	Search_data* sd = &td->ss;
	S_SearchINFO* info = &td->info;
	PvTable* pv_table = &td->pv_table;
	// Play 5 random moves
	if (random_moves < 5) {
		make_random_move(pos);
		random_moves++;
		return;
	}
	//Get position fen
	std::string pos_fen = get_fen(pos);
	//Search best move and get score
	int score = search_best_move(td, ss);
	//Get best move
	getBestMove(pv_table);

	return;
}
