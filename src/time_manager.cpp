#include "time_manager.h"
#include "search.h"
#include <algorithm>
#include "misc.h"

//Calculate how much time to spend on searching a move
void optimum(S_SearchINFO* info, int time, int inc) {

	//if we recieved a movetime command we need to spend exactly that amount of time on the move, so we don't scale
	if (info->movetimeset) 
	{
		int safety_overhead = 50;
		time -= safety_overhead;
		info->stoptimeMax = info->starttime + time;
		info->stoptimeOpt = info->starttime + time;
	}
	//else If we recieved a movestogo parameter we use total_time/movestogo
	else if (info->timeset && info->movestogo != -1)
	{
		int safety_overhead = 50;
		time -= safety_overhead;
		int time_slot = time / info->movestogo;
		int basetime = (time_slot);
		info->stoptimeMax = info->starttime + basetime;
		info->stoptimeOpt = info->starttime + basetime;
	}
	// else if we recieved wtime/btime we calculate an over and upper bound for the time usage based on fixed coefficients
	else if (info->timeset)
	{
		int safety_overhead = 50;
		time -= safety_overhead;
		int time_slot = time / 20 + inc / 2;
		int basetime = (time_slot);
		//optime is the time we use to stop if we just cleared a depth
		int optime = basetime * 0.6;
		//maxtime is the absolute maximum time we can spend on a search
		int maxtime = std::min(time, basetime * 2);
		info->stoptimeMax = info->starttime + maxtime;
		info->stoptimeOpt = info->starttime + optime;
	}



}

bool stopEarly(const S_SearchINFO* info) 
{
	// check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
	if ((info->timeset || info->movetimeset) && GetTimeMs() > info->stoptimeOpt)
		return true;
	else return false;
}

bool nodesOver(const S_SearchINFO* info) {
	// check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
	if (info->nodeset == TRUE && info->nodes > info->nodeslimit)
		return true;
	else return false;
}

bool timeOver(const S_SearchINFO* info) {
	// check if more than Maxtime passed and we have to stop
	if (((info->timeset || info->movetimeset)
		&& ((info->nodes & 1023) == 1023)
		&& GetTimeMs() > info->stoptimeMax))
		return true;
	else return false;
}