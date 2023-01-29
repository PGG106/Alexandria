#pragma once
extern bool do_datagen;
void datagen(S_ThreadData* td, Search_stack* ss);
struct data_entry {
	std::string fen;
	int score;
	int wdl;
};
//Takes a board as an input and determines if the game is over
bool is_game_over(S_Board* pos);