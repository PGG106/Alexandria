#pragma once

#include <cstdint>
#include <string>

struct S_Board;
struct S_SearchINFO;

struct S_UciOptions {
    uint64_t Hash = 16;
    int MultiPV = 1;
    int Threads = 1;
    bool datagen = false;
};
// Internal flag to decide if to pretty or ugly print search results
extern bool print_uci;
// Parse a move from algebraic notation to the internal value
int ParseMove(const std::string& move_string, S_Board* pos);
// parse UCI "position" command
void ParsePosition(const std::string& command, S_Board* pos);

// parse UCI "go" command
bool ParseGo(const std::string& line, S_SearchINFO* info, S_Board* pos);

// main UCI loop
void UciLoop(char** argv);
