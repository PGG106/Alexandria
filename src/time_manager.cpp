#include "time_manager.h"

#include "search.h"
#include "uci.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

//Calculate how much time to spend on searching a move
void optimum(S_Board* pos, S_SearchINFO* info, int time, int inc) {
	//if we recieved a time parameter from the gui
	if (time != -1) {
		info->timeset = TRUE;
		//If we recieved a movestogo parameter
		if (info->movestogo != -1) {
			//time is equal to (total time)/ number of moves we have to play - a safety overhead
			time /= info->movestogo;
			time -= 50;
			int stoptimeBase = (info->starttime + (time + inc / 2));
			info->stoptimeOpt = stoptimeBase * 0.6;
			info->stoptimeMax = stoptimeBase * 2;
		}
		else
		{
		int  stoptimeBase = (info->starttime + (time / 20 + inc / 2));
		info->stoptimeOpt = stoptimeBase * 0.6;
		info->stoptimeMax = stoptimeBase * 2;
		}
	}

	return;
}
