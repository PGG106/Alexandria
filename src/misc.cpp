

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

