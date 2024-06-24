#pragma once

#include <cstdint>
#include <string>

struct Position;
struct SearchInfo;

struct UciOptions {
    uint64_t Hash = 16;
    static constexpr int MultiPV = 1;
    int Threads = 1;
};

// Internal flag to decide if to pretty or ugly print search results
inline bool print_uci = false;
// Internal flag to disable the output of search results when we don't want our speed to be limited by the console
inline bool tryhardmode = false;
// Parse a move from algebraic notation to the internal value
[[nodiscard]] int ParseMove(const std::string& move_string, Position* pos);
// parse UCI "position" command
void ParsePosition(const std::string& command, Position* pos);

// parse UCI "go" command
[[nodiscard]] bool ParseGo(const std::string& line, SearchInfo* info, Position* pos);

// main UCI loop
void UciLoop(int argc, char** argv);
