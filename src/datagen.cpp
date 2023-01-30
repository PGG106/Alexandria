#include "Board.h"
#include "movegen.h"
#include <time.h>
#include "makemove.h"
#include "datagen.h"
#include <iostream>
#include <fstream>
#include "misc.h"
#include "threads.h"
#include "ttable.h"
int total_fens = 0;
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


//Starts the search process, this is ideally the point where you can start a multithreaded search
void Root_datagen(S_ThreadData* td, Datagen_params params)
{
	//Resize TT to an appropriate size
	InitHashTable(HashTable, 16 * params.threadnum);

	//Init a thread_data object for each helper thread that doesn't have one already
	for (int i = threads_data.size(); i < params.threadnum - 1;i++)
	{
		threads_data.emplace_back();
		threads_data.back().id = i + 1;
	}

	//Init thread_data objects
	for (size_t i = 0; i < threads_data.size();i++)
	{
		threads_data[i].info = td->info;
		threads_data[i].pos = td->pos;
	}

	// Start Threads-1 helper search threads
	for (int i = 0; i < params.threadnum - 1;i++)
	{
		threads.emplace_back(std::thread(datagen, &threads_data[i], params));
	}

	//MainThread datagen
	datagen(td, params);
	std::cout << "Datagen done!\n";
}


void datagen(S_ThreadData* td, Datagen_params params)
{

	//Each thread gets its own file to dump data into
	std::ofstream myfile("data" + std::to_string(td->id) + ".txt", std::ios_base::app);
	auto start_time = GetTimeMs();
	if (myfile.is_open())
	{
		for (int i = 0;i < params.games;i++)
		{
			play_game(td, myfile);
			if (!(i % 1000))
				std::cout << total_fens << " fens completed" << " current speed is " << total_fens * 1000 / (GetTimeMs() - start_time) << " fens per second\n";
		}
		myfile.close();
	}
	else std::cout << "Unable to open file";
	return;

}

void play_game(S_ThreadData* td, std::ofstream& myfile)
{
	S_Board* pos = &td->pos;
	PvTable* pv_table = &td->pv_table;
	init_new_game(td);
	// Play 6 random moves
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
		//Get if the position is in check
		bool in_check = IsInCheck(pos, pos->side);
		//Search best move and get score
		entry.score = pos->side == WHITE ? search_best_move(td) : -search_best_move(td);
		//Get best move
		int move = getBestMove(pv_table);
		//play the move
		make_move(move, pos);
		//We don't save the position if the best move is a capture
		if (get_move_capture(move)) continue;
		//We don't save the position if the score is a mate score
		if (abs(entry.score) > ISMATE) continue;
		//If we were in check we discard the position
		if (in_check) continue;
		//If we are at an early ply skip the position
		if (pos->hisPly < 8) continue;
		//Add the entry to the vector waiting for the wdl
		entries.push_back(entry);

	}
	//When the game is over
	total_fens += entries.size();
	//Dump to file
	for (data_entry entry : entries)
		myfile << entry.fen << " " << wdl << " " << entry.score << "\n";
	return;
}

bool is_game_over(S_Board* pos, std::string& wdl)
{
	//Check for draw
	if (IsDraw(pos))
	{
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
		wdl = in_check ? pos->side == WHITE ? "[0.0]" : "[1.0]" : "[0.5]";
		return true;
	}

	return false;
}
