#include "Board.h"
#include "movegen.h"
#include <time.h>
#include <stdlib.h>
#include "makemove.h"
#include "datagen.h"
#include <iostream>
#include <fstream>

int random_moves = 0;
bool do_datagen = true;
enum line_type {
	result,
	fen,
	moves

};
void make_random_move(S_Board* pos, S_Stack* ss, S_SearchINFO* info) {

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



void datagen(S_Board* pos, S_Stack* ss, S_SearchINFO* info)
{
	srand(time(NULL));
	// Play 5 random moves
	if (random_moves < 5) {
		Sleep(250);
		make_random_move(pos, ss, info);
		random_moves++;
		return;
	}

	//From this point onwards search as normal

	//variable used to store the score of the best move found by the search (while the move itself can be retrieved from the TT)
	int score = 0;

	//Clean the position and the search info to start search from a clean state 
	ClearForSearch(pos, ss, info);

	// define initial alpha beta bounds
	int alpha = -MAXSCORE;
	int beta = MAXSCORE;
	// Call the negamax function in an iterative deepening framework
	for (int current_depth = 1; current_depth <= info->depth; current_depth++)
	{
		score = negamax(alpha, beta, current_depth, pos, ss, info, TRUE);

		// check if we just cleared a depth and more than OptTime passed
		if (info->timeset && GetTimeMs() > info->stoptimeOpt)
			info->stopped = true;

		if (info->stopped)
			// stop calculating and return best move so far
			break;


		// print new line
		printf("\n");
	}

	printf("bestmove ");
	print_move(ss->pvArray[0][0]);
	printf("\n");

	return;
}

int get_line_type(std::string line) {
	if (line[0] != '[')
		return moves;
	else if (line[1] != 'R')
		return fen;
	else return result;
}

void convert_pgn_to_format(std::string stripped_pgn_path) {

	std::ifstream myfile;
	myfile.open(stripped_pgn_path);
	std::string line;
	std::string game_Score;
	double move_Score = 0;
	std::string current_fen = start_position;
	std::string move;
	size_t pos = 0;
	char delimiter = ' ';
	if (myfile.is_open())
	{
		while (std::getline(myfile, line))
		{//skip over any blank line
			if (line.length() == 0) continue;
			//get line type
			int type = get_line_type(line);
			if (type == result)
			{
				game_Score = parse_result(line);
			}

			else if (type == fen) {
				current_fen = get_fen(line);
			}
			//TODO make it loop to the right point
			else if (type == moves)
			{
				//remove duplicate result field at the end of the move sequence
				line.erase(line.find_last_of(delimiter), line.length());
				while (1) {
					pos = line.find(delimiter);
					//if there are no more spaces it means we got all the moves
					if (pos == std::string::npos) break;
					//get the played move
					move = line.substr(0, pos);
					//remove it from the list
					line.erase(0, pos + 1);
					pos = line.find(delimiter);
					//extra variable to check if it's a mate before doing stod (otherwise it crashes)
					std::string xyz = line.substr(0, pos);
					//if it contains an M then it's a mate score, break
					if (xyz.find('M') != std::string::npos) break;
					move_Score = stod(xyz);
					// don't use |scores| > 2000
					if (abs(move_Score) > 2000) break;
					//remove from the list
					line.erase(0, pos + 1);
					//Save result either to file or in datastructure
					std::cout << current_fen << " [" << game_Score << "] " << move_Score * 100 << std::endl;
					//update fen for next iter
				}
			}

		}
		myfile.close();
	}
	else {
		std::cout << "could not open the file";
	}

}

std::string parse_result(std::string line) {
	if (line.at(9) == '0') return "0.0";
	else if (line.at(10) == '/') return "0.5";
	else return "1.0";
}

std::string get_fen(std::string line) {
	int offset = line.find(" \" ") + 2;
	return line.substr(offset, line.length() - 8);
}
