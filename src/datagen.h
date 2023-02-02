#pragma once
#include "threads.h"

//The internal structure of a "fen" worth of training data, in Cudad format
struct data_entry {
	std::string fen;
	int score;
	std::string wdl;
};
//Parameters unique to datagen that the user might want to override
struct Datagen_params
{
	int threadnum = 6;
	//The games are defined per thread and not as a cumulative sum
	int games = 100000;
};

//Root datagen function that handles the start-up of datagen
void Root_datagen(S_ThreadData* td, Datagen_params params);
//Per thread datagen function
void datagen(S_ThreadData* td, int games_number);
//Generates one game worth of fens
bool play_game(S_ThreadData* td, std::ofstream& myfile);

//Takes a board as an input and determines if the game is over
bool is_game_over(S_Board* pos, std::string& wdl);