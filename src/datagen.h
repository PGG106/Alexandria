#pragma once
#include "threads.h"
#include <atomic>
extern std::atomic<bool> stop_flag;
//The internal structure of a "fen" worth of training data, in Cudad format
struct data_entry {
	std::string fen;
	int score;
	std::string wdl;
};
//Parameters unique to Datagen that the user might want to override
struct Datagen_params
{
	int threadnum = 6;
	//The games are defined per thread and not as a cumulative sum
	int games = 100000;
};

//Root Datagen function that handles the start-up of Datagen
void RootDatagen(S_ThreadData* td, Datagen_params params);
//Per thread Datagen function
void Datagen(S_ThreadData* td, Datagen_params params);
//Generates one game worth of fens
bool PlayGame(S_ThreadData* td, std::ofstream& myfile);

//Takes a board as an input and determines if the game is over
bool IsGameOver(S_Board* pos, std::string& wdl);