#pragma once

#include <atomic>
#include <string>

struct S_ThreadData;
struct S_Board;

extern std::atomic<bool> stop_flag;
// The internal structure of a "fen" worth of training data, in Cudad format
struct data_entry {
    std::string fen;
    int score;
    std::string wdl;
};
// Parameters unique to Datagen that the user might want to override
struct Datagen_params
{
    int threadnum = 6;
    // The games are defined per thread and not as a cumulative sum
    uint64_t games = 10000000;
};

// Root Datagen function that handles the start-up of Datagen
void RootDatagen(S_ThreadData* td, Datagen_params params);
// Per thread Datagen function
void Datagen(S_ThreadData* td, Datagen_params params);
// Generates one game worth of fens
[[nodiscard]] bool PlayGame(S_ThreadData* td, std::ofstream& myfile, uint64_t& total_fens);

// Takes a board as an input and determines if the game is over
[[nodiscard]] bool IsGameOver(S_Board* pos, std::string& wdl);
