#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "move.h"
#include "Board.h"
#include "makemove.h"
#include "misc.h"
#include "movegen.h"
#include "io.h"
#include "search.h"
#include "ttable.h"
#include "time_manager.h"
#include "bench.h"
#include <iostream>

int parse_move(char* move_string, S_Board* pos)
{
	// create move list instance
	S_MOVELIST move_list[1];

	// generate moves
	generate_moves(move_list, pos);

	// parse source square
	int source_square = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;

	// parse target square
	int target_square = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;

	// loop over the moves within a move list
	for (int move_count = 0; move_count < move_list->count; move_count++)
	{
		// init move
		int move = move_list->moves[move_count].move;

		// make sure source & target squares are available within the generated move
		if (source_square == get_move_source(move) && target_square == get_move_target(move))
		{
			// init promoted piece
			int promoted_piece = get_move_promoted(move);

			// promoted piece is available
			if (promoted_piece)
			{
				// promoted to queen
				if ((promoted_piece == WQ || promoted_piece == BQ) && move_string[4] == 'q')
					// return legal move
					return move;

				// promoted to rook
				else if ((promoted_piece == WR || promoted_piece == BR) && move_string[4] == 'r')
					// return legal move
					return move;

				// promoted to bishop
				else if ((promoted_piece == WB || promoted_piece == BB) && move_string[4] == 'b')
					// return legal move
					return move;

				// promoted to knight
				else if ((promoted_piece == WN || promoted_piece == BN) && move_string[4] == 'n')
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

/*
	Example UCI commands to init position on chess board

	// init start position
	position startpos

	// init start position and make the moves on chess board
	position startpos moves e2e4 e7e5

	// init position from FEN string
	position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1

	// init position from fen string and make moves on chess board
	position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 moves e2a6 e8g8
*/

// parse UCI "position" command
void parse_position(char* command, S_Board* pos)
{
	// shift pointer to the right where next token begins
	command += 9;

	// init pointer to the current character in the command string
	char* current_char = command;

	// parse UCI "startpos" command
	if (strncmp(command, "startpos", 8) == 0)
		// init chess board with start position
		parse_fen(start_position, pos);

	// parse UCI "fen" command 
	else
	{
		// make sure "fen" command is available within command string
		current_char = strstr(command, "fen");

		// if no "fen" command is available within command string
		if (current_char == NULL)
			// init chess board with start position
			parse_fen(start_position, pos);

		// found "fen" substring
		else
		{
			// shift pointer to the right where next token begins
			current_char += 4;

			// init chess board with position from FEN string
			parse_fen(current_char, pos);
		}
	}

	// parse moves after position
	current_char = strstr(command, "moves");

	// moves available
	if (current_char != NULL)
	{
		// shift pointer to the right where next token begins
		current_char += 6;

		// loop over moves within a move string
		while (*current_char)
		{
			// parse next move
			int move = parse_move(current_char, pos);

			// if no more moves
			if (move == 0)
				// break out of the loop
				break;

			// make move on the chess board
			make_move(move, pos);

			// move current character mointer to the end of current move
			while (*current_char && *current_char != ' ') current_char++;

			// go to the next move
			current_char++;
		}
	}

	// print board
	print_board(pos);
}

/*
	Example UCI commands to make engine search for the best move

	// fixed depth search
	go depth 64
*/

// parse UCI "go" command
void parse_go(char* line, S_SearchINFO* info, S_Board* pos) {

	int depth = -1, movetime = -1;
	int movestogo;
	int time = -1, inc = 0;
	char* ptr = NULL;
	info->timeset = FALSE;
	info->starttime = 0;
	info->stoptime = 0;
	info->depth = 0,
		info->timeset = 0;

	if ((ptr = strstr(line, "infinite"))) {
		;
	}

	if ((ptr = strstr(line, "binc")) && pos->side == BLACK) {
		inc = atoi(ptr + 5);
	}

	if ((ptr = strstr(line, "winc")) && pos->side == WHITE) {
		inc = atoi(ptr + 5);
	}

	if ((ptr = strstr(line, "wtime")) && pos->side == WHITE) {
		time = atoi(ptr + 6);
	}

	if ((ptr = strstr(line, "btime")) && pos->side == BLACK) {
		time = atoi(ptr + 6);
	}

	if ((ptr = strstr(line, "movestogo"))) {
		movestogo = atoi(ptr + 10);
		if (movestogo > 0)
			info->movestogo = movestogo;
	}

	if ((ptr = strstr(line, "movetime"))) {
		movetime = atoi(ptr + 9);
	}

	if ((ptr = strstr(line, "depth"))) {
		depth = atoi(ptr + 6);
	}

	if (movetime != -1) {
		time = movetime;
		info->movestogo = 1;
	}

	info->starttime = GetTimeMs();
	info->depth = depth;

	info->stoptime = optimum(pos, info, time, inc);


	if (depth == -1) {
		info->depth = MAXDEPTH;
	}

	printf("time:%d start:%d stop:%d depth:%d timeset:%d\n",
		time, info->starttime, info->stoptime, info->depth, info->timeset);

	Root_search_position(info->depth, pos, info);
}



/*
	GUI -> isready
	Engine -> readyok
	GUI -> ucinewgame
*/

// main UCI loop
void Uci_Loop(S_Board* pos, S_SearchINFO* info, char** argv)
{

	if (argv[1] && strncmp(argv[1], "bench", 5) == 0) {
		start_bench();
		return;

	}

	int MB = 64;

	// reset STDIN & STDOUT buffers
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	// define user / GUI input buffer
	char input[2000];

	// print engine info
	printf("id name Alexandria\n");
	printf("id author PGG\n");
	printf("option name Hash type spin default 64 \n");
	printf("Type nnue to enable/disable nnue eval (default is enabled) \n");
	printf("uciok\n");

	// main loop
	while (1)
	{
		// reset user /GUI input
		memset(input, 0, sizeof(input));

		// make sure output reaches the GUI
		fflush(stdout);

		// get user / GUI input
		if (!fgets(input, 2000, stdin))
			// continue the loop
			continue;

		// make sure input is available
		if (input[0] == '\n')
			// continue the loop
			continue;

		// parse UCI "isready" command
		if (strncmp(input, "isready", 7) == 0)
		{
			printf("readyok\n");
			continue;
		}

		// parse UCI "position" command
		else if (strncmp(input, "position", 8) == 0)
			// call parse position function
			parse_position(input, pos);

		// parse UCI "ucinewgame" command
		else if (strncmp(input, "ucinewgame", 10) == 0) {

			ClearHashTable(HashTable);

			// call parse position function
			parse_position((char*)"position startpos", pos);

			Reset_info(info);


		}
		// parse UCI "go" command
		else if (strncmp(input, "go", 2) == 0)
			// call parse go function
			parse_go(input, info, pos);

		// parse UCI "quit" command
		else if (strncmp(input, "quit", 4) == 0)
			// quit from the chess engine program execution
			break;

		// parse UCI "uci" command
		else if (strncmp(input, "uci", 3) == 0)
		{
			// print engine info
			printf("id name Alexandria\n");
			printf("id author PGG106\n");
			printf("uciok\n");
		}


		else if (strncmp(input, "eval", 4) == 0)
		{
			// print engine info
			printf("the eval of this position according to the neural network is %d\n", nnue.output());
		}


		else if (strncmp(input, "nnue", 4) == 0)
		{
			// print engine info
			nnue_eval ^= 1;
			std::cout << std::boolalpha;
			std::cout << "nnue is now set to " << nnue_eval << "\n";
		}

		else if (!strncmp(input, "setoption name Hash value ", 26)) {
			sscanf(input, "%*s %*s %*s %*s %d", &MB);
			printf("Set Hash to %d MB\n", MB);
			InitHashTable(HashTable, MB);
		}
		else if (strncmp(input, "bench", 5) == 0) {


			start_bench();

			return;

		}


	}
}
