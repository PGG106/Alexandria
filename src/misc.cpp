

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

void PrintUciOutput(int score, int depth, S_SearchINFO* info, S_UciOptions* options) {

	//This handles the basic console output
	unsigned long  time = GetTimeMs() - info->starttime;
	uint64_t nps = info->nodes / (time + !time) * 1000;

	if (score > -mate_value && score < -mate_score)
		printf("info score mate %d depth %d seldepth %d multipv %d nodes %lu nps %lld time %lld pv ",
			-(score + mate_value) / 2, depth, info->seldepth, options->MultiPV, info->nodes, nps,
			GetTimeMs() - info->starttime);

	else if (score > mate_score && score < mate_value)
		printf("info score mate %d depth %d seldepth %d multipv %d nodes %lu nps %lld time %lld pv ",
			(mate_value - score) / 2 + 1, depth, info->seldepth, options->MultiPV, info->nodes, nps,
			GetTimeMs() - info->starttime);

	else
		printf("info score cp %d depth %d seldepth %d multipv %d nodes %lu nps %lld time %lld pv ",
			score, depth, info->seldepth, options->MultiPV, info->nodes, nps, GetTimeMs() - info->starttime);
	return;
}
