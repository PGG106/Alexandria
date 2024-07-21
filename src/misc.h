#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include "makemove.h"
#include "io.h"
#include "eval.h"

[[nodiscard]] inline uint64_t GetTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline long long int _count, _accumulator;

// splits a string into a vector of tokens and returns it
[[nodiscard]] inline std::vector<std::string> split_command(const std::string& command) {
    std::stringstream stream(command);
    std::string intermediate;
    std::vector<std::string> tokens;

    while (std::getline(stream, intermediate, ' ')) {
        tokens.push_back(intermediate);
    }

    return tokens;
}

// returns true if in a vector of string there's one that matches the key
[[nodiscard]] inline bool Contains(const std::vector<std::string>& tokens, const std::string& key) {
    return std::find(tokens.begin(), tokens.end(), key) != tokens.end();
}

inline void dbg_mean_of(int val) { _count++; _accumulator += val; }

inline void dbg_print() { std::cout << double(_accumulator) / _count << std::endl; }

inline void debugUE(const std::string& fen, int move, Position* pos){
    ParseFen(fen, pos);
    std::cout<<"\n Starting values: \n";
    for(int i = 0; i < 10; i++) {
        std::cout << pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].values[i]<<" ";
    }
    std::cout<<std::endl;

    const auto piece = Piece(move);
    const auto fromSquare = From(move);
    const auto toSquare = To(move);
    const bool flip = KingSQ(pos,WHITE) > 3;

    std::cout<<"Correct Indexes: " <<pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].GetIndex(piece,fromSquare,flip)<<" " << pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].GetIndex(piece,toSquare,flip) << std::endl;
    MakeMove<true>(move,pos);
    std::cout << "Used Index: " << pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].NNUESub[0]<< " " << pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].NNUEAdd[0] << std::endl;

    bool refresh_required =  pos->accumStack[pos->accumStackHead-1].perspective[WHITE].needsRefresh;

    std::cout << "Refresh needed: " << refresh_required<<std::endl;

    std::cout<<" Applied changes:\n";
    auto Add = &net.FTWeights[pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].NNUEAdd[0] * L1_SIZE];
    auto Sub = &net.FTWeights[pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].NNUESub[0] * L1_SIZE];

    for (int i = 0; i < 10; i++) {
        std::cout << Sub[i]<<" ";
    }

    std::cout<<std::endl;

    for (int i = 0; i < 10; i++) {
        std::cout << Add[i]<<" ";
    }
    std::cout <<std::endl;

    std::cout<<" changes according to accumulate:\n";

    auto Add_index = pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].GetIndex(piece,toSquare,flip);
    auto correct_Add = &net.FTWeights[Add_index * L1_SIZE];

    auto Sub_index = pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].GetIndex(piece,fromSquare,flip);
    auto correct_Sub = &net.FTWeights[Sub_index * L1_SIZE];

    for (int i = 0; i < 10; i++) {
        std::cout << correct_Sub[i] << " ";
    }

    std::cout <<std::endl;

    for (int i = 0; i < 10; i++) {
        std::cout << correct_Add[i]  << " ";
    }

    std::cout <<std::endl;

    int eval = EvalPositionRaw(pos);

    std::cout<<" Final values samples:\n";
    for(int i = 0; i < 10; i++) {
        std::cout << pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].values[i]<<" ";
    }
    std::cout<<std::endl;

    int correct_eval = 0;

    nnue.accumulate(pos->AccumulatorTop(), pos);

    for(int i = 0; i < 10; i++) {
        std::cout << pos->accumStack[pos->accumStackHead - 1].perspective[WHITE].values[i]<<" ";
    }

    bool stm = pos->side == WHITE;
    int pieceCount = (pos)->PieceCount();
    int outputBucket = std::min((63 - pieceCount) * (32 - pieceCount) / 225, 7);
    correct_eval = nnue.output(pos->accumStack[pos->accumStackHead -1], stm, outputBucket);

    if(eval != correct_eval){
        PrintBoard(pos);
        std::cout << eval <<std::endl;
        std::cout << correct_eval <<std::endl;
    }
    assert(eval == correct_eval);

}
