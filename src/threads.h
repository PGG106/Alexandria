#pragma once
#include <vector>
#include <thread>
#include "search.h"

// global vector of search threads
extern std::vector<std::thread> threads;
// global vector of thread_datas
extern std::vector<S_ThreadData> threads_data;

uint64_t getTotalNodes(const int threadcount);
void stopHelperThreads();
void stopHelperThreadsDatagen();

