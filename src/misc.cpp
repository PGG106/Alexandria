

#include "Board.h"
#include "stdio.h"
#include "stdlib.h"
#ifdef _WIN64
#include "windows.h"
#include <io.h>
#else
#include "string.h"
#include "sys/select.h"
#include "sys/time.h"
#include "unistd.h"
#endif
#include "io.h"
#include "perft.h"
#include "search.h"

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
	unsigned long  time = GetTimeMs() - td->info.starttime;
	uint64_t nps = td->info.nodes / (time + !time) * 1000;

	if (score > -mate_value && score < -mate_score)
		printf("info score mate %d depth %d seldepth %d multipv %d nodes %lu nps %lld time %lld pv ",
			-(score + mate_value) / 2, depth, td->info.seldepth, options->MultiPV, td->info.nodes, nps,
			GetTimeMs() - td->info.starttime);

	else if (score > mate_score && score < mate_value)
		printf("info score mate %d depth %d seldepth %d multipv %d nodes %lu nps %lld time %lld pv ",
			(mate_value - score) / 2 + 1, depth, td->info.seldepth, options->MultiPV, td->info.nodes, nps,
			GetTimeMs() - td->info.starttime);

	else
		printf("info score cp %d depth %d seldepth %d multipv %d nodes %lu nps %lld time %lld pv ",
			score, depth, td->info.seldepth, options->MultiPV, td->info.nodes, nps, GetTimeMs() - td->info.starttime);

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
