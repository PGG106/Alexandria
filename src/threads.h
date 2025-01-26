#pragma once

#include <cstdint>
#include <vector>
#include <thread>
#include "search.h"

enum state {
    Idle,
    Search,
};
// global vector of search threads
inline std::vector<std::thread> threads;
// global vector of thread_datas
inline std::vector<ThreadData> threads_data;

[[nodiscard]] inline uint64_t GetTotalNodes() {
    uint64_t nodes = 0ULL;
    for (const auto& td : threads_data) {
        nodes += td.nodes;
    }
    return nodes;
}

inline void StopHelperThreads() {
    // Stop helper threads
    setStop(true);

    for (auto& th : threads) {
        if (th.joinable())
            th.join();
    }

    threads.clear();
}