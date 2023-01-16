#pragma once
#include "Board.h"

typedef struct OPTIONS {
	uint64_t Hash = 16;
	int MultiPV = 1;
	int Threads = 1;
	bool datagen = false;
} S_UciOptions;
//Internal flag to decide if to pretty or ugly print search results
extern bool print_uci;
int parse_move(const std::string& move_string, S_Board* pos);
// parse UCI "position" command
void parse_position(const std::string& command, S_Board* pos);

// parse UCI "go" command
bool parse_go(const std::string& line, S_SearchINFO* info, S_Board* pos);

// main UCI loop
void Uci_Loop(char** argv);