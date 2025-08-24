#pragma once

#include <cstdint>
#include <vector>
#include <thread>
#include "history.h"
#include "position.h"

enum state {
    Idle,
    Search,
};

// Hold the data from the uci input to set search parameters and some search data to populate the uci output
struct SearchInfo {
    // search start time
    uint64_t starttime = 0;
    // search time initial lower bound if present
    uint64_t stoptimeBaseOpt = 0;
    // search time scaled lower bound if present
    uint64_t stoptimeOpt = 0;
    // search upper bound if present
    uint64_t stoptimeMax = 0;
    // max depth to reach for depth limited searches
    int depth = -1;
    int seldepth = -1;
    // types of search limits
    bool timeset = false;
    bool nodeset = false;
    bool movetimeset = false;

    int movestogo = 0;
    uint64_t nodes = 0;
    uint64_t nodeslimit = 0;

    bool stopped = false;

    inline void Reset() {
        depth = 0;
        nodes = 0;
        starttime = 0;
        stoptimeOpt = 0;
        stoptimeMax = 0;
        movestogo = 0;
        stopped = false;
        timeset = false;
        movetimeset = false;
        nodeset = false;
    }

    inline void print(){
        std::cout << "start: " << starttime << " ";
        std::cout << "stopOpt: " << stoptimeOpt << " ";
        std::cout << "stopMax: " << stoptimeMax << " ";
        std::cout << "depth: " << depth << " ";
        std::cout << "timeset: " << timeset << " ";
        std::cout << "nodeset: " << nodeset << std::endl;
    }
};

struct SearchData {
    int searchHistory[2][64 * 64] = {};
    int rootHistory[2][64 * 64] = {};
    int pawnHist[12 * 64][PH_SIZE] = {};
    int captHist[12 * 64][6] = {};
    Move counterMoves[64 * 64] = {};
    int contHist[12 * 64][12 * 64] = {};
    int pawnCorrHist[2][CORRHIST_SIZE] = {};
    int whiteNonPawnCorrHist[2][CORRHIST_SIZE] = {};
    int blackNonPawnCorrHist[2][CORRHIST_SIZE] = {};
    int contCorrHist[2][6 * 64][6 * 64] = {};
};

// a collection of all the data a thread needs to conduct a search
struct ThreadData {
    int id = 0;
    Position pos;
    SearchData sd;
    SearchInfo info;
    int RootDepth;
    int nmpPlies;

    NNUE::FinnyTable FTable{};

    inline void resetFinnyTable() {
        FTable = NNUE::FinnyTable{};
    }
};

// global vector of search threads
inline std::vector<std::thread> threads;
// global vector of thread_datas
inline std::vector<ThreadData> threads_data;

[[nodiscard]] inline uint64_t GetTotalNodes() {
    uint64_t nodes = 0ULL;
    for (const auto& td : threads_data) {
        nodes += td.info.nodes;
    }
    return nodes;
}

inline void StopHelperThreads() {
    // Stop helper threads
    for (auto& td : threads_data) {
        td.info.stopped = true;
    }

    for (auto& th : threads) {
        if (th.joinable())
            th.join();
    }

    threads.clear();
}
