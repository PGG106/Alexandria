#pragma once

#include <cstdint>
#include <vector>
#include <thread>
#include "search.h"

enum state {
    Idle,
    Search,
    datagen
};
// global vector of search threads
extern std::vector<std::thread> threads;
// global vector of thread_datas
extern std::vector<S_ThreadData> threads_data;

uint64_t GetTotalNodes(const int threadcount);
void StopHelperThreads();
