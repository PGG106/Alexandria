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
extern std::vector<ThreadData> threads_data;

[[nodiscard]] uint64_t GetTotalNodes();
void StopHelperThreads();
