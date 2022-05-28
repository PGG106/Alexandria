#pragma once


#include "stdio.h"
#include "Board.h"
#include "search.h"
#ifdef _WIN32
#include "windows.h"
#else
#include "sys/time.h"
#include "sys/select.h"
#include "unistd.h"
#include "string.h"
#endif


int GetTimeMs();

int InputWaiting();







const char* getfield(char* line, int num);



void runtestpositions(FILE* file, int depth);




void ReadInput(S_SearchINFO* info);