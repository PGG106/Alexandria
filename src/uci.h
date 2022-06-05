#pragma once
#include "Board.h"



int parse_move(char* move_string, S_Board* pos);
// parse UCI "position" command
void parse_position(char* command, S_Board* pos);


// parse UCI "go" command
void parse_go(char* line, S_SearchINFO* info, S_Board* pos);

// main UCI loop
void Uci_Loop(S_Board* pos, S_SearchINFO* info, char** argv);