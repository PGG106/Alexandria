#include "board.h"
#include "movegen.h"
#include <time.h>
#include "makemove.h"
#include "datagen.h"
#include <iostream>
#include <fstream>
#include "misc.h"
#include "ttable.h"
#include "history.h"
#include "time_manager.h"

unsigned long long total_fens = 0;
std::atomic<bool> stop_flag = false;
void make_random_move(S_Board* pos) {
	srand(time(NULL));
	S_MOVELIST move_list[1];
	// generate moves
	GenerateMoves(move_list, pos);
	//In the extremely rare case that we have mate/stale during the random moves
	if (move_list->count == 0)
		return;
	//We have at least one move
	assert(move_list->count >= 1);
	int r = rand() % move_list->count;
	assert(r >= 0 && r < move_list->count);
	int random_move = move_list->moves[r].move;
	make_move(random_move, pos);
	return;
}

void set_new_game_state(S_ThreadData* td) {

	//Extract data structures from ThreadData
	S_Board* pos = &td->pos;
	Search_data* ss = &td->ss;
	S_SearchINFO* info = &td->info;
	PvTable* pv_table = &td->pv_table;

	CleanHistories(ss);

	//Clean the Pv array
	for (int index = 0; index < MAXDEPTH + 1; ++index) {
		pv_table->pvLength[index] = 0;
		for (int index2 = 0; index2 < MAXDEPTH + 1; ++index2) {
			pv_table->pvArray[index][index2] = NOMOVE;
		}
	}

	//Clean the Counter moves array
	for (int index = 0; index < Board_sq_num; ++index) {
		for (int index2 = 0; index2 < Board_sq_num; ++index2) {
			ss->CounterMoves[index][index2] = NOMOVE;
		}
	}

	//Reset plies and search info
	pos->ply = 0;
	info->starttime = GetTimeMs();
	info->stopped = 0;
	info->nodes = 0;
	info->seldepth = 0;
	pos->fiftyMove = 0;

	//delete played moves hashes
	pos->played_positions.clear();

	// call parse position function
	ParsePosition("position startpos", pos);
	return;

}

//Does an high depth search of a position to confirm that it's sane enough to use for Datagen
int sanity_search(S_ThreadData* td)
{
	Search_stack stack[MAXDEPTH], * ss = stack;
	//variable used to store the score of the best move found by the search (while the move itself can be retrieved from the TT)
	int score = 0;
	//Clean the position and the search info to start search from a clean state 
	ClearForSearch(td);
	// define initial alpha beta bounds
	int alpha = -MAXSCORE;
	int beta = MAXSCORE;
	score = Negamax(alpha, beta, 10, td, ss);

	return score;
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
	// Call the Negamax function in an iterative deepening framework
	for (int current_depth = 1; current_depth <= info->depth; current_depth++)
	{
		score = Negamax(alpha, beta, current_depth, td, ss);

		// check if we just cleared a depth and we used the nodes we had we stop
		if (NodesOver(&td->info))
			info->stopped = true;

		if (info->stopped)
			// stop calculating and return best move so far
			break;

	}
	return score;
}


//Starts the search process, this is ideally the point where you can start a multithreaded search
void RootDatagen(S_ThreadData* td, Datagen_params params)
{
	std::cout << "Starting datagen with " << params.threadnum << " threads and an estimated total of " << params.games * params.threadnum << " games" << std::endl;
	//Resize TT to an appropriate size
	InitHashTable(HashTable, 32 * params.threadnum);

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
		threads.emplace_back(std::thread(Datagen, &threads_data[i], params));
	}

	//MainThread Datagen
	Datagen(td, params);
	std::cout << "Waiting for the other threads to finish\n";
	//Wait for helper threads to finish
	StopHelperThreads();

	std::cout << "Datagen done!\n";
}


void Datagen(S_ThreadData* td, Datagen_params params)
{
	//Each thread gets its own file to dump data into
	auto start_time = GetTimeMs();
	std::ofstream myfile("data" + std::to_string(td->id) + ".txt", std::ios_base::app);
	if (myfile.is_open())
	{
		if (td->id == 0)
			std::cout << "Datagen started successfully" << std::endl;
		for (int i = 1;i <= params.games;i++)
		{
			if (stop_flag)
			{
				if (td->id == 0)
					std::cout << "Stopping Datagen..\n";
				break;
			}
			//Make sure a game is started on a clean state
			set_new_game_state(td);
			//Restart if we get a busted random score
			if (!PlayGame(td, myfile))
			{
				i--;
				continue;
			}
			if (td->id == 0 && !(i % 100))
				std::cout << i * params.threadnum << " games completed total_fens: " << total_fens << " speed: " << (total_fens * 1000 / (1 + GetTimeMs() - start_time)) << " fens/s" << std::endl;
		}
		myfile.close();
	}
	else std::cout << "Unable to open file";
	return;

}

bool PlayGame(S_ThreadData* td, std::ofstream& myfile)
{
	S_Board* pos = &td->pos;
	PvTable* pv_table = &td->pv_table;
	for (int i = 0;i < 6; i++)
	{
		make_random_move(pos);
	}
	//Pre emptively check the position to see if it's to be discarderd
	int sanity_score = sanity_search(td);
	if (abs(sanity_score) > 1000) return false;
	//container to store all the data entries before dumping them to a file
	std::vector<data_entry> entries;
	//String for wdl
	std::string wdl;
	//if the game is over we also get the wdl to avoid having to check twice
	while (!IsGameOver(pos, wdl))
	{
		//Get a data entry
		data_entry entry;
		//Get position fen
		entry.fen = GetFen(pos);
		//Get if the position is in check
		bool in_check = IsInCheck(pos, pos->side);
		//Search best move and get score
		entry.score = pos->side == WHITE ? search_best_move(td) : -search_best_move(td);
		//Get best move
		int move = GetBestMove(pv_table);
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
	return true;
}

bool IsGameOver(S_Board* pos, std::string& wdl)
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
	GenerateMoves(move_list, pos);
	//Check for mate or stalemate
	if (move_list->count == 0)
	{
		bool in_check = IsInCheck(pos, pos->side);
		wdl = in_check ? pos->side == WHITE ? "[0.0]" : "[1.0]" : "[0.5]";
		return true;
	}
	//Sanity check if the game somehow gets stuck in a loop
	if (pos->ply > 1500) {
		wdl = "[0.5]";
		return true;
	}
	return false;
}
