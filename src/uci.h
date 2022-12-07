#pragma once
#include "Board.h"

typedef struct OPTIONS {
	uint64_t Hash=16;
	int MultiPV=1;
	bool datagen=false;
} S_UciOptions;


int parse_move(char *move_string, S_Board *pos);
// parse UCI "position" command
void parse_position(char *command, S_Board *pos);

// parse UCI "go" command
void parse_go(char *line, S_SearchINFO *info, S_Board *pos);

// main UCI loop
void Uci_Loop(S_Board* pos, S_Stack* ss, S_SearchINFO* info, char** argv);