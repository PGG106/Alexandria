#pragma once

#include <map>

// convert ASCII character pieces to encoded constants
extern std::map<char, int> char_pieces;

// promoted pieces
extern std::map<int, char> promoted_pieces;