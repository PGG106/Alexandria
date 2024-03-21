#include "threads.h"

// global vector of search threads
std::vector<std::thread> threads;
// global vector of thread_datas
std::vector<ThreadData> threads_data;

uint64_t GetTotalNodes() {
    uint64_t nodes = 0ULL;
    for (const auto& td : threads_data) {
        nodes += td.info.nodes;
    }
    return nodes;
}

void StopHelperThreads() {
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
