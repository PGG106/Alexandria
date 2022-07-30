#include "time_manager.h"

#include "search.h"
#include "uci.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#define BASE_DIV 20
//Calculate how much time to spend on searching a move
int optimum(S_Board* pos, S_SearchINFO* info, int time, int inc) {

	//if we recieved a time parameter from the gui
	if (time != -1) {

		info->timeset = TRUE;
		//If we recieved a movestogo parameter
		if (info->movestogo != -1 && info->movestogo < BASE_DIV) {
			time = (std::max)(time / BASE_DIV, time / info->movestogo - 50);
			return (info->starttime + (time + inc / 2));
		}
		//if not we just use the time remaining/20
		else return (info->starttime + (time / 20 + inc / 2));
	}

	return (info->starttime + (time + inc / 2));
}
