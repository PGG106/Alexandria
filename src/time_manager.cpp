#include "time_manager.h"

#include "search.h"
#include "uci.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include "misc.h"

//Calculate how much time to spend on searching a move
void optimum(S_SearchINFO* info, int time, int inc) {

	//if we recieved a movetime command we need to spend exactly that amount of time on the move, so we don't scale
	if (info->movetimeset) {
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

bool stopEarly(const S_SearchINFO* info) {
	// check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
	if (((info->timeset || info->movetimeset) && GetTimeMs() > info->stoptimeOpt)
		|| (info->nodeset == TRUE && info->nodes > info->nodeslimit))
		return true;
	else return false;
}

bool timeOver(const S_SearchINFO* info) {
	// check if more than Maxtime passed and we have to stop
	if (((info->timeset || info->movetimeset)
		&& ((info->nodes & 1023) == 1023)
		&& GetTimeMs() > info->stoptimeMax)
		|| (info->nodeset == TRUE && info->nodes > info->nodeslimit))
		return true;
	else return false;
}

bool QuickReturn(const S_Stack* ss, const S_SearchINFO* info)
{
	if (!info->timeset) return false;
	//Get the best move
	int bestmove = getBestMove(ss);
	//get the minimum duration a normal search iteration would have
	int minimum_search_duration = (info->stoptimeOpt - info->starttime);
	//Calculate the reduction
	int bestMoveNodePercent = (ss->NodesMove[From(bestmove)][To(bestmove)] * 100) / info->nodes;
	float time_scaling_factor = 50.0 / std::max(bestMoveNodePercent, 25);
	assert(bestMoveNodePercent >= 0);
	//Get new minimum duration 
	int reduced_min_time = minimum_search_duration * time_scaling_factor;
	//If more time than the minimum search duration passed then stop search
	int elapsed = GetTimeMs() - info->starttime;
	if (elapsed > reduced_min_time)
		return true;

	return false;
}
