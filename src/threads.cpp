#include "threads.h"

// global vector of search threads
std::vector<std::thread> threads;
// global vector of thread_datas
std::vector<S_ThreadData> threads_data;

uint64_t GetTotalNodes(const int threadcount) {
	uint64_t nodes = 0ULL;
	for (int i = 0; i < threadcount - 1;i++) {
		nodes += threads_data[i].info.nodes;
	}
	return nodes;
}

void StopHelperThreads() {
	//Stop helper threads
	for (size_t i = 0; i < threads_data.size();i++)
	{
		threads_data[i].info.stopped = true;
	}

	for (std::thread& th : threads)
	{
		if (th.joinable())
			th.join();
	}

	threads.clear();

}
