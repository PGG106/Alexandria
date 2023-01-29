#include "Board.h"
#include "movegen.h"
#include <time.h>
#include <stdlib.h>
#include "makemove.h"
#include "datagen.h"
#include <iostream>
#include <fstream>
#include "misc.h"

void make_random_move(S_Board* pos) {
	srand(time(NULL));
	S_MOVELIST move_list[1];
	// generate moves
	generate_moves(move_list, pos);
	int r = rand() % move_list->count;
	int random_move = move_list->moves[r].move;
	make_move(random_move, pos);
	return;
}

int search_best_move(S_ThreadData* td)
{
	S_SearchINFO* info = &td->info;
	Search_stack stack[MAXDEPTH], * ss = stack;
	//variable used to store the score of the best move found by the search (while the move itself can be retrieved from the TT)
	int score = 0;

	//Clean the position and the search info to start search from a clean state 
	ClearForSearch(td);

	// define initial alpha beta bounds
	int alpha = -MAXSCORE;
	int beta = MAXSCORE;
	// Call the negamax function in an iterative deepening framework
	for (int current_depth = 1; current_depth <= 6; current_depth++)
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


void datagen(S_ThreadData* td)
{
	S_Board* pos = &td->pos;
	PvTable* pv_table = &td->pv_table;
	parse_fen(start_position, pos);
	// Play 10 random moves
	for (int i = 0;i < 6; i++)
	{
		ClearForSearch(td);
		make_random_move(pos);
	}

	//container to store all the data entries before dumping them to a file
	std::vector<data_entry> entries;
	//String for wdl
	std::string wdl;
	//if the game is over we also get the wdl to avoid having to check twice
	while (!is_game_over(pos, wdl))
	{
		//Get a data entry
		data_entry entry;
		//Get position fen
		entry.fen = get_fen(pos);
		//Search best move and get score
		ClearForSearch(td);
		entry.score = pos->side == WHITE ? search_best_move(td) : -search_best_move(td);
		//Add the entry to the vector waiting for the wdl
		entries.push_back(entry);
		//Get best move
		int move = getBestMove(pv_table);
		//play the move
		make_move(move, pos);
	}
	//When the game is over

	//Dump to file
	for (data_entry entry : entries)
		std::cout << entry.fen << " " << wdl << " " << entry.score << "\n";
	return;
}

bool is_game_over(S_Board* pos, std::string& wdl)
{
	//Check for draw
	if (IsDraw(pos)) {
		wdl = "[0.5]";
		return true;
	}
	// create move list instance
	S_MOVELIST move_list[1];
	// generate moves
	generate_moves(move_list, pos);
	//Check for mate or stalemate
	if (move_list->count == 0)
	{
		bool in_check = IsInCheck(pos, pos->side);
		// if the king is in check return mating score (assuming closest distance to mating position) otherwise return stalemate 
		wdl = in_check ? pos->side == WHITE ? "[0.0]" : "[1.0]" : "[0.5]";
		return true;
	}

	return false;
}
