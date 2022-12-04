#include "Board.h"
#pragma once
extern bool do_datagen;
void datagen(S_Board* pos, S_Stack* ss, S_SearchINFO* info);
void convert_pgn_to_format(std::string stripped_pgn_path);
int get_line_type(std::string line);
std::string parse_result(std::string line);
std::string get_fen(std::string line);