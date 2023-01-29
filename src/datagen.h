#pragma once
extern bool do_datagen;
void datagen(S_ThreadData* td);

struct data_entry {
	std::string fen;
	int score;
	std::string wdl;
};
//Takes a board as an input and determines if the game is over
bool is_game_over(S_Board* pos, std::string& wdl);