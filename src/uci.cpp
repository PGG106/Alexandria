#include "Board.h"
#include "bench.h"
#include "io.h"
#include "uci.h"
#include "makemove.h"
#include "misc.h"
#include "move.h"
#include "types.h"
#include "movegen.h"
#include "perft.h"
#include "search.h"
#include "time_manager.h"
#include "ttable.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include "datagen.h"
#include "threads.h"

//convert a move to coordinate notation to internal notation
int parse_move(const std::string& move_string, S_Board* pos) {
	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	generate_moves(move_list, pos);

	// parse source square
	const int source_square = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;

	// parse target square
	const int target_square = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;

	// loop over the moves within a move list
	for (int move_count = 0; move_count < move_list->count; move_count++) {
		// init move
		const int move = move_list->moves[move_count].move;

		// make sure source & target squares are available within the generated move
		if (source_square == From(move) &&
			target_square == To(move)) {
			// init promoted piece
			const int promoted_piece = get_move_promoted(move);

			// promoted piece is available
			if (promoted_piece) {
				// promoted to queen
				if ((promoted_piece == WQ || promoted_piece == BQ) &&
					move_string[4] == 'q')
					// return legal move
					return move;

				// promoted to rook
				else if ((promoted_piece == WR || promoted_piece == BR) &&
					move_string[4] == 'r')
					// return legal move
					return move;

				// promoted to bishop
				else if ((promoted_piece == WB || promoted_piece == BB) &&
					move_string[4] == 'b')
					// return legal move
					return move;

				// promoted to knight
				else if ((promoted_piece == WN || promoted_piece == BN) &&
					move_string[4] == 'n')
					// return legal move
					return move;

				// continue the loop on possible wrong promotions (e.g. "e7e8f")
				continue;
			}

			// return legal move
			return move;
		}
	}

	// return illegal move
	return 0;
}

// parse UCI "position" command
void parse_position(const std::string& command, S_Board* pos) {

	// parse UCI "startpos" command
	if (command.find("startpos") != std::string::npos) {
		// init chess board with start position
		parse_fen(start_position, pos);
	}

	// parse UCI "fen" command
	else {

		// if a "fen" command is available within command string
		if (command.find("fen") != std::string::npos) {
			// init chess board with position from FEN string
			parse_fen(command.substr(command.find("fen") + 4, std::string::npos), pos);
		}
		else {
			// init chess board with start position
			parse_fen(start_position, pos);
		}

	}

	// if there are moves to be played in the fen play them
	if (command.find("moves") != std::string::npos) {
		int string_start = command.find("moves") + 6;
		std::string moves_substr = command.substr(string_start, std::string::npos);
		parse_moves(moves_substr, pos);
	}

	//Update accumulator state to reflect the new position
	accumulate(pos->accumulator, pos);
}

/*
		Example UCI commands to make engine search for the best move

		// fixed depth search
		go depth 64
*/

// parse UCI "go" command
void parse_go(const std::string& line, S_SearchINFO* info, S_Board* pos) {
	Reset_info(info);
	int depth = -1, movetime = -1;
	int movestogo;
	int time = -1, inc = 0;

	std::vector<std::string> tokens = split_command(line);

	//loop over all the tokens and parse the commands
	for (size_t i = 1; i < tokens.size();i++) {

		if (tokens.at(1) == "infinite") {
			;
		}

		if (tokens.at(1) == "perft") {
			int perft_depth = std::stoi(tokens[1]);
			perft_test(perft_depth, pos);
			return;
		}

		if (tokens.at(i) == "binc" && pos->side == BLACK) {
			inc = std::stoi(tokens[i + 1]);
		}

		if (tokens.at(i) == "winc" && pos->side == WHITE) {
			inc = std::stoi(tokens[i + 1]);
		}

		if (tokens.at(i) == "wtime" && pos->side == WHITE) {
			time = std::stoi(tokens[i + 1]);
			info->timeset = true;
		}
		if (tokens.at(i) == "btime" && pos->side == BLACK) {
			time = std::stoi(tokens[i + 1]);
			info->timeset = true;
		}

		if (tokens.at(i) == "movestogo") {
			movestogo = std::stoi(tokens[i + 1]);
			if (movestogo > 0)
				info->movestogo = movestogo;
		}

		if (tokens.at(i) == "movetime") {
			movetime = std::stoi(tokens[i + 1]);
			time = movetime;
			info->movetimeset = true;
		}


		if (tokens.at(i) == "depth") {
			depth = std::stoi(tokens[i + 1]);
		}

		if (tokens.at(i) == "nodes") {
			info->nodeset = true;
			info->nodeslimit = std::stoi(tokens[i + 1]);
		}
	}

	info->starttime = GetTimeMs();
	info->depth = depth;
	//calculate time allocation for the move
	optimum(info, time, inc);

	if (depth == -1) {
		info->depth = MAXDEPTH;
	}

	std::cout << "time: " << time << " ";
	std::cout << "start: " << info->starttime << " ";
	std::cout << "stopOpt: " << info->stoptimeOpt << " ";
	std::cout << "stopMax: " << info->stoptimeMax << " ";
	std::cout << "depth: " << info->depth << " ";
	std::cout << "timeset: " << info->timeset << " ";
	std::cout << "nodeset: " << info->nodeset << "\n";

}

// main UCI loop
void Uci_Loop(char** argv) {
	if (argv[1] && strncmp(argv[1], "bench", 5) == 0) {
		start_bench();
		return;
	}

	bool parsed_position = false;
	S_UciOptions uci_options[1];
	S_ThreadData* td(new ThreadData());
	std::thread main_search_thread;
	// print engine info
	printf("id name Alexandria 4.0-dev\n");

	// main loop
	while (1) {

		// define user / GUI input buffer
		std::string input;

		// get user / GUI input
		if (!std::getline(std::cin, input)) {
			// continue the loop
			break;
		}

		// make sure input is available
		if (!input.length()) {
			// continue the loop
			continue;
		}

		//Split the string into tokens to make it easier to work with
		std::vector<std::string> tokens = split_command(input);

		// parse UCI "position" command
		if (tokens[0] == "position") {
			// call parse position function
			parse_position(input, &td->pos);
			parsed_position = true;
		}

		// parse UCI "go" command
		else if (tokens[0] == "go") {

			stopHelperThreads();
			//Join previous search thread if it exists
			if (main_search_thread.joinable())
				main_search_thread.join();

			if (!parsed_position) // call parse position function
			{
				parse_position("position startpos", &td->pos);
			}
			// call parse go function
			parse_go(input, &td->info, &td->pos);
			// Start search in a separate thread
			main_search_thread = std::thread(Root_search_position, td->info.depth, td, uci_options);
		}

		if (tokens[0] == "setoption") {

			if (tokens[2] == "Hash") {
				uci_options->Hash = std::stoi(tokens[4]);
				std::cout << "Set Hash to " << uci_options->Hash << " MB\n";
				InitHashTable(HashTable, uci_options->Hash);
			}
			else if (tokens[2] == "Threads") {
				uci_options->Threads = std::stoi(tokens[4]);
				std::cout << "Set Threads to " << uci_options->Threads << "\n";
			}
		}

		// parse UCI "isready" command
		if (input == "isready") {
			std::cout << "readyok\n";

			continue;
		}

		// parse UCI "ucinewgame" command
		else if (input == "ucinewgame") {
			init_new_game(&td->pos, &td->ss, &td->info);
		}
		// parse UCI "stop" command
		else if (input == "stop") {
			//Stop helper threads
			stopHelperThreads();
			//stop main thread search
			td->info.stopped = true;
		}

		// parse UCI "quit" command
		else if (input == "quit" || input == "exit") {
			//Stop helper threads
			stopHelperThreads();
			//stop main thread search
			td->info.stopped = true;

			//Join previous search thread if it exists
			if (main_search_thread.joinable())
				main_search_thread.join();
			//free thread data
			delete td;
			// quit from the chess engine program execution
			break;
		}

		// parse UCI "uci" command
		else if (input == "uci") {
			// print engine info
			printf("id name Alexandria 4.0-dev\n");
			printf("id author PGG\n");
			printf("option name Hash type spin default 16 min 1 max 8192 \n");
			printf("option name Threads type spin default 1 min 1 max 256 \n");
			//printf("option name MultiPV type spin default 1 min 1 max 1\n");
			printf("uciok\n");
		}

		// parse UCI "uci" command
		else if (input == "d") {
			print_board(&td->pos);
		}

		else if (input == "eval") {
			// print engine info
			printf(
				"the eval of this position according to the neural network is %d\n",
				nnue.output(td->pos.accumulator));
		}

		else if (input == "bench") {
			start_bench();
		}

		else if (input == "see") {
			// create move list instance
			S_MOVELIST move_list[1];

			// generate moves
			generate_moves(move_list, &td->pos);
			printf("SEE thresholds\n");
			for (int i = 0; i < move_list->count;i++) {
				int move = move_list->moves[i].move;
				for (int j = 1200;j > -1200;j--) {
					if (SEE(&td->pos, move, j)) {
						printf(" move number %d  %s SEE result: %d \n", i + 1, FormatMove(move), j);
						break;
					}
				}
			}
		}
	}
}
