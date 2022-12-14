#include "threads.h"

// global vector of search threads
std::vector<std::thread> threads;
// global vector of thread_datas
std::vector<S_ThreadData> threads_data;

uint64_t getTotalNodes(int threadcount) {
	uint64_t nodes = 0ULL;
	for (int i = 0; i < threadcount - 1;i++) {
		nodes += threads_data[i].info.nodes;
	}
	return nodes;
}

void stopHelperThreads() {
	//Stop helper threads
	for (int i = 0; i < threads_data.size() - 1;i++)
	{
		threads_data[i].info.stopped = true;
	}
}