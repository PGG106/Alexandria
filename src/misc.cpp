

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

int InputWaiting() {
#ifndef WIN32
	fd_set readfds;
	struct timeval tv;
	FD_ZERO(&readfds);
	FD_SET(fileno(stdin), &readfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	select(16, &readfds, 0, 0, &tv);

	return (FD_ISSET(fileno(stdin), &readfds));
#else
	static int init = 0, pipe;
	static HANDLE inh;
	DWORD dw;

	if (!init) {
		init = 1;
		inh = GetStdHandle(STD_INPUT_HANDLE);
		pipe = !GetConsoleMode(inh, &dw);
		if (!pipe) {
			SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(inh);
		}
	}
	if (pipe) {
		if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL))
			return 1;
		return dw;
	}
	else {
		GetNumberOfConsoleInputEvents(inh, &dw);
		return dw <= 1 ? 0 : dw;
	}
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

/*
void runtestpositions(FILE* file, int depth) {
	char line[1024];
	char position[100];
	unsigned long long expectednodes;
	unsigned long long actualnodes;
	S_Board pos[1];
	while (fgets(line, 1024, file))
	{
		char* tmp = _strdup(line);
		char* tmp2 = _strdup(line);
		strcpy(position, getfield(tmp, 1));

		printf("position: %s ", position);


		char* nodesfield = getfield(tmp2, depth + 1);
		strtok(nodesfield, " ");
		char* nodestoken = strtok(NULL, " ");


		expectednodes = strtoull(nodestoken, '\0', 10);
		printf(" expected nodes: %llu ", expectednodes);



		parse_fen(position, pos);
		actualnodes = perft_test(depth, pos);
		if (expectednodes != actualnodes) {
			printf("PERFT ERROR\n");
			printf("expected nodes differ from actual nodes %llu != %llu",
expectednodes, actualnodes); return;
		}

		free(tmp);
		free(tmp2);


	}
}


*/

void ReadInput(S_SearchINFO* info) {
	int bytes;
	char input[256] = "", * endc;

	if (InputWaiting()) {
		info->stopped = TRUE;
		do {
			bytes = read(fileno(stdin), input, 256);
		} while (bytes < 0);
		endc = strchr(input, '\n');
		if (endc)
			*endc = 0;

		if (strlen(input) > 0) {
			if (!strncmp(input, "quit", 4)) {
				info->quit = TRUE;
			}
		}
		return;
	}
}