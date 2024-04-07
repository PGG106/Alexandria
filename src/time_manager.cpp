#include "time_manager.h"
#include "position.h"
#include "search.h"
#include <algorithm>
#include "misc.h"

// Calculate how much time to spend on searching a move
void Optimum(SearchInfo* info, int time, int inc) {
    // If ccrl sent us a negative time just assume we have a workable amount of time to search for a move
    if (time < 0) time = 1000;
    // Reserve some time overhead to avoid timing out in the engine-gui communication process
    const int safety_overhead = std::min(300, time / 2);
    time -= safety_overhead;
    // if we recieved a movetime command we need to spend exactly that amount of time on the move, so we don't scale
    if (info->movetimeset) {
        info->stoptimeMax = info->starttime + time;
        info->stoptimeOpt = info->starttime + time;
    }
    // else If we recieved a movestogo parameter we use total_time/movestogo
    else if (info->timeset && info->movestogo != -1) {
        // Divide the time you have left for how many moves you have to play
        const auto basetime = time / info->movestogo;
        // Never use more than 76% of the total time left for a single move
        const auto maxtimeBound = 0.76 * time;
        // optime is the time we use to stop if we just cleared a depth
        const auto optime = std::min(0.76 * basetime, maxtimeBound);
        // maxtime is the absolute maximum time we can spend on a search (unless it is bigger than the bound)
        const auto maxtime = std::min(3.04 * basetime, maxtimeBound);
        info->stoptimeMax = info->starttime + maxtime;
        info->stoptimeBaseOpt = optime;
        info->stoptimeOpt = info->starttime + info->stoptimeBaseOpt;
    }
    // else if we recieved wtime/btime we calculate an over and upper bound for the time usage based on fixed coefficients
    else if (info->timeset) {
        int basetime = time * 0.054 + inc * 0.85;
        // Never use more than 76% of the total time left for a single move
        const auto maxtimeBound = 0.76 * time;
        // optime is the time we use to stop if we just cleared a depth
        const auto optime = std::min(0.76 * basetime, maxtimeBound);
        // maxtime is the absolute maximum time we can spend on a search (unless it is bigger than the bound)
        const auto maxtime = std::min(3.04 * basetime, maxtimeBound);
        info->stoptimeMax = info->starttime + maxtime;
        info->stoptimeBaseOpt = optime;
        info->stoptimeOpt = info->starttime + info->stoptimeBaseOpt;
    }
}

bool StopEarly(const SearchInfo* info) {
    // check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
    return (info->timeset || info->movetimeset) && GetTimeMs() > info->stoptimeOpt;
}

void ScaleTm(ThreadData* td, const int bestMoveStabilityFactor) {
    constexpr double bestmoveScale[5] = {2.43, 1.35, 1.09, 0.88, 0.68};
    const int bestmove = GetBestMove(&td->pvTable);
    // Calculate how many nodes were spent on checking the best move
    const double bestMoveNodesFraction = static_cast<double>(td->nodeSpentTable[FromTo(bestmove)]) / static_cast<double>(td->info.nodes);
    const double nodeScalingFactor = (1.52 - bestMoveNodesFraction) * 1.74;
    const double bestMoveScalingFactor = bestmoveScale[bestMoveStabilityFactor];
    // Scale the search time based on how many nodes we spent and how the best move changed
    td->info.stoptimeOpt = std::min<uint64_t>(td->info.starttime + td->info.stoptimeBaseOpt * nodeScalingFactor * bestMoveScalingFactor, td->info.stoptimeMax);

}

bool NodesOver(const SearchInfo* info) {
    // check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
    return info->nodeset && info->nodes > info->nodeslimit;
}

bool TimeOver(const SearchInfo* info) {
    // check if more than Maxtime passed and we have to stop
    return NodesOver(info) || ((info->timeset || info->movetimeset)
                               && ((info->nodes & 1023) == 1023)
                               && GetTimeMs() > info->stoptimeMax);
}
