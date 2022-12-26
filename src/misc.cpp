

#include "Board.h"
#include "stdio.h"
#include "stdlib.h"
#include "search.h"
#include "io.h"
#ifdef _WIN64
#include "windows.h"
#else
#include "string.h"
#include "sys/select.h"
#include "sys/time.h"
#include "unistd.h"
#endif
#include "threads.h"
#include <iostream>


int GetTimeMs() {
#ifdef WIN32
	return GetTickCount64();
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
#endif
}

const char* getfield(char* line, int num) {
	const char* tok;
	for (tok = strtok(line, ";"); tok && *tok; tok = strtok(NULL, ";")) {
		if (!--num)
			return tok;
	}
	return NULL;
}

void PrintUciOutput(int score, int depth, S_ThreadData* td, S_UciOptions* options) {

	//This handles the basic console output
	uint64_t  time = GetTimeMs() - td->info.starttime;
	uint64_t nodes = td->info.nodes + getTotalNodes(options->Threads);

	uint64_t nps = nodes / (time + !time) * 1000;

	if (score > -mate_value && score < -mate_score)
		std::cout << "info score mate " << -(score + mate_value) / 2 << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
		" nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

	else if (score > mate_score && score < mate_value)
		std::cout << "info score mate " << (mate_value - score) / 2 + 1 << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
		" nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

	else
		std::cout << " info score cp " << score << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
		" nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

	// loop over the moves within a PV line
	for (int count = 0; count < td->ss.pvLength[0]; count++) {
		// print PV move
		print_move(td->ss.pvArray[0][count]);
		printf(" ");
	}

	// print new line
	printf("\n");

	return;
}
