#include "time_manager.h"
#include "position.h"
#include "search.h"
#include <algorithm>
#include "misc.h"

// Calculate how much time to spend on searching a move
void Optimum(SearchInfo *info, int time, int inc) {
    // If ccrl sent us a negative time just assume we have a workable amount of time to search for a move
    if (time < 0) time = 1000;
    // Reserve some time overhead to avoid timing out in the engine-gui communication process
    const int safety_overhead = std::min(15, time / 2);
    // if we received a movetime command we need to spend exactly that amount of time on the move, so we don't scale
    if (info->movetimeset) {
        info->stoptimeMax = info->starttime + time;
        info->stoptimeOpt = info->starttime + time;
        return;
    }
    const bool cyclicTC = info->movestogo != 0;
    const int movesToGo = cyclicTC ? info->movestogo : 50;
    const auto timeLeft =  std::max(1, time + inc * (movesToGo - 1) - safety_overhead * (2 + movesToGo));
    double optScale = 0;
    if(cyclicTC){
        optScale = std::min(0.90 / movesToGo, 0.88 * time / double(timeLeft));
    }
    else{
        optScale = std::min(0.020, 0.20 * time / double(timeLeft));
    }
    // optime is the time we use to stop if we just cleared a depth
    const auto optime = optScale * timeLeft;
    info->stoptimeBaseOpt = optime;
    info->stoptimeOpt = info->starttime + info->stoptimeBaseOpt;
    // Never use more than 76% of the total time left for a single move
    const auto maxtime = 0.76 * time;
    info->stoptimeMax = info->starttime + maxtime - safety_overhead;
}

bool StopEarly(const SearchInfo* info) {
    // check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
    return (info->timeset || info->movetimeset) && GetTimeMs() > info->stoptimeOpt;
}

void ScaleTm(ThreadData* td, const int bestMoveStabilityFactor, const int evalStabilityFactor) {
    const double bestmoveScale[5] = {bmScale1() / 100.0, bmScale2() / 100.0, bmScale3() / 100.0, bmScale4() / 100.0, bmScale5() / 100.0};
    const double evalScale[5] = {evalScale1() / 100.0, evalScale2() / 100.0, evalScale3() / 100.0, evalScale4() / 100.0, evalScale5() / 100.0};
    const int bestmove = GetBestMove();
    // Calculate how many nodes were spent on checking the best move
    const double bestMoveNodesFraction = static_cast<double>(nodeSpentTable[FromTo(bestmove)]) / static_cast<double>(td->info.nodes);
    const double nodeScalingFactor = (nodeTmBase() / 100.0 - bestMoveNodesFraction) * (nodeTmMultiplier() / 100.0);
    const double bestMoveScalingFactor = bestmoveScale[bestMoveStabilityFactor];
    const double evalScalingFactor = evalScale[evalStabilityFactor];
    // Scale the search time based on how many nodes we spent and how the best move changed
    td->info.stoptimeOpt = std::min<uint64_t>(td->info.starttime + td->info.stoptimeBaseOpt * nodeScalingFactor * bestMoveScalingFactor * evalScalingFactor, td->info.stoptimeMax);

}

bool NodesOver(const SearchInfo* info) {
    // check if we used all the nodes/movetime we had or if we used more than our lowerbound of time
    return info->nodeset && info->nodes >= info->nodeslimit;
}

bool TimeOver(const SearchInfo* info) {
    // check if more than Maxtime passed and we have to stop
    return NodesOver(info) || ((info->timeset || info->movetimeset)
                               && ((info->nodes & 1023) == 1023)
                               && GetTimeMs() > info->stoptimeMax);
}
