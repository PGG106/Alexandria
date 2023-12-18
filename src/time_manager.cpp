#include "time_manager.h"
#include "board.h"
#include "search.h"
#include <algorithm>
#include "misc.h"

// Calculate how much time to spend on searching a move
void Optimum(S_SearchINFO* info, int time, int inc) {
    // If ccrl sent us a negative time just assume we have a workable amount of time to search for a move
    if (time < 0) time = 1000;
    // Reserve some time overhead to avoid timing out in the engine-gui communication process
    int safety_overhead = 100;
    time -= safety_overhead;
    // if we recieved a movetime command we need to spend exactly that amount of time on the move, so we don't scale
    if (info->movetimeset) {
        info->stoptimeMax = info->starttime + time;
        info->stoptimeOpt = info->starttime + time;
    }
    // else If we recieved a movestogo parameter we use total_time/movestogo
    else if (info->timeset && info->movestogo != -1) {
        // Divide the time you have left for how many moves you have to play
        auto basetime = time / info->movestogo;
        // Never use more than 80% of the total time left for a single move
        auto maxtimeBound = 0.75 * time;
        // optime is the time we use to stop if we just cleared a depth
        auto optime = std::min(0.7 * basetime, maxtimeBound);
        // maxtime is the absolute maximum time we can spend on a search (unless it is bigger than the bound)
        auto maxtime = std::min(3.0 * basetime, maxtimeBound);
        info->stoptimeMax = info->starttime + maxtime;
        info->stoptimeBaseOpt = optime;
        info->stoptimeOpt = info->starttime + info->stoptimeBaseOpt;
    }
    // else if we recieved wtime/btime we calculate an over and upper bound for the time usage based on fixed coefficients
    else if (info->timeset) {
        int basetime = time / 20 + inc * 3 / 4;
        // optime is the time we use to stop if we just cleared a depth
        int optime = basetime * 0.6;
        // maxtime is the absolute maximum time we can spend on a search
        int maxtime = std::min(time, basetime * 2);
        info->stoptimeMax = info->starttime + maxtime;
        info->stoptimeBaseOpt = optime;
        info->stoptimeOpt = info->starttime + info->stoptimeBaseOpt;
    }
}

bool StopEarly(const S_SearchINFO* info) {
    // check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
    return (info->timeset || info->movetimeset) && GetTimeMs() > info->stoptimeOpt;
}

void ScaleTm(S_ThreadData* td, const int bestmoveStabilityFactor) {
    constexpr double bestmoveScale[5] = { 2.0, 1.2, 0.95, 0.85, 0.8 };
    int bestmove = GetBestMove(&td->pvTable);
    // Calculate how many nodes were spent on checking the best move
    double bestMoveNodesFraction = static_cast<double>(td->nodeSpentTable[From(bestmove)][To(bestmove)]) / static_cast<double>(td->info.nodes);
    double nodeScalingFactor = (1.62 - bestMoveNodesFraction) * 1.48;
    // Scale the search time based on how many nodes we spent
    td->info.stoptimeOpt = std::min<uint64_t>(td->info.starttime + td->info.stoptimeBaseOpt * nodeScalingFactor * bestmoveScale[bestmoveStabilityFactor], td->info.stoptimeMax);

}

bool NodesOver(const S_SearchINFO* info) {
    // check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
    return info->nodeset && info->nodes > info->nodeslimit;
}

bool TimeOver(const S_SearchINFO* info) {
    // check if more than Maxtime passed and we have to stop
    return NodesOver(info) || ((info->timeset || info->movetimeset)
                               && ((info->nodes & 1023) == 1023)
                               && GetTimeMs() > info->stoptimeMax);
}
