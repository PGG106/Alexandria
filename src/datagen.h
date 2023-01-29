#pragma once
extern bool do_datagen;
//Root datagen function that handles the start-up of datagen
void Root_datagen(S_ThreadData* td);
//Root datagen function that handles the start-up of datagen
void datagen(S_ThreadData* td, int number_of_games = 100000000);
//Generates one game worth of fens
void playgame(S_ThreadData* td, std::ofstream& myfile);

struct data_entry {
	std::string fen;
	int score;
	std::string wdl;
};
//Takes a board as an input and determines if the game is over
bool is_game_over(S_Board* pos, std::string& wdl);