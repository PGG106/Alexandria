#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>

/*
How tuning works in alex, a brief summary:
To add a variable for tuning we call the addTune function in initTunables, this will do 2 things
1) create a "tunable param" object with all the info we have to feed to the OB spsa input, this info can be dumped using the uci command "tune"
2) add the parameter to an unordered map.
3) do some cursed getter macro wizardry with the TUNE_PARAM macro
*/

// Very cursed macro wizardry to set and fetch the values without having to manually swap in hashtable accesses, it's good because Ciekce did this
// Start with the case where we are actually tuning
#ifdef TUNE
#define STRINGIFY(x) #x
#define HELPER1(x) STRINGIFY(GCC diagnostic ignored x)
#define GCC_WARNING(x) _Pragma(HELPER1(x))
#define TUNE_PARAM(Name, Default, Min, Max, C_end, R_end) \
        _Pragma("GCC diagnostic push") \
        GCC_WARNING("-Wdangling-reference")               \
        inline const int& tuned_##Name = addTune(#Name, Default, Default, Min, Max, C_end, R_end); \
        _Pragma("GCC diagnostic pop") \
        inline int Name() { return tuned_##Name; }
#else
#define TUNE_PARAM(Name, Default, Min, Max, C_end, R_end) \
        constexpr int Name() { return Default; }
#endif

#ifdef TUNE_TM
#define STRINGIFY(x) #x
#define HELPER1(x) STRINGIFY(GCC diagnostic ignored x)
#define GCC_WARNING(x) _Pragma(HELPER1(x))
#define TUNE_TM_PARAM(Name, Default, Min, Max, C_end, R_end) \
        _Pragma("GCC diagnostic push") \
        GCC_WARNING("-Wdangling-reference")               \
        inline const int& tuned_##Name = addTune(#Name, Default, Default, Min, Max, C_end, R_end); \
        _Pragma("GCC diagnostic pop") \
        inline int Name() { return tuned_##Name; }
#else
#define TUNE_TM_PARAM(Name, Default, Min, Max, C_end, R_end) \
        constexpr int Name() { return Default; }
#endif



// This class acts as a fancy string constructor, it's used just to store all the info OB wants for a tune
struct tunable_param {
    std::string name;
    int defaultValue;
    int currValue;
    int minValue;
    int maxValue;
    float C_end;
    float R_end;

    friend std::ostream& operator<<(std::ostream& os, const tunable_param& param)
    {
        os << param.name << ", ";
        os << "int, ";
        os << param.currValue << ", ";
        os << param.minValue << ", ";
        os << param.maxValue << ", ";
        os << param.C_end << ", ";
        os << param.R_end;
        return os;
    }
};
inline std::unordered_map<std::string, tunable_param> &tunableParams()
{
    static std::unordered_map<std::string, tunable_param> tunableParams{};
    return tunableParams;
}

inline std::vector<tunable_param> &tunables()
{
    static std::vector<tunable_param> tunables{};
    return tunables;
}

// Actual functions to init and update variables
inline const int &addTune(std::string name, int defaultValue, int curr_value, int min_value, int max_value, float C_end, float R_end)
{
    tunable_param &param = tunableParams()[name];
    param = tunable_param{name, defaultValue, curr_value, min_value, max_value, C_end, R_end};
    tunables().push_back(param);
    return param.currValue;
}

// Handles the update of a variable being tuned
inline bool updateTuneVariable(std::string tune_variable_name, int value)
{
    auto iter = tunableParams().find(tune_variable_name);

    if (iter == tunableParams().end())
    {
        return false;
    }

    iter->second.currValue = value;
    return true;
}

// TM STUFF
// SOFT/HARD bounds
TUNE_TM_PARAM(optScaleFixed, 25, 10, 55, 2, 0.002)
TUNE_TM_PARAM(optScaleTimeLeft, 200, 75, 350, 15, 0.002)

// Bestmove stability
TUNE_TM_PARAM(bmScale1, 238, 50, 300, 10, 0.002)
TUNE_TM_PARAM(bmScale2, 129, 50, 200, 10, 0.002)
TUNE_TM_PARAM(bmScale3, 107, 50, 150, 6, 0.002)
TUNE_TM_PARAM(bmScale4, 91, 40, 110, 5, 0.002)
TUNE_TM_PARAM(bmScale5, 71, 35, 100, 5, 0.002)

// Eval stability
TUNE_TM_PARAM(evalScale1, 125, 90, 160, 4, 0.002)
TUNE_TM_PARAM(evalScale2, 115, 80, 150, 4, 0.002)
TUNE_TM_PARAM(evalScale3, 103, 80, 150, 4, 0.002)
TUNE_TM_PARAM(evalScale4, 92, 60, 130, 4, 0.002)
TUNE_TM_PARAM(evalScale5, 87, 40, 110, 4, 0.002)

// Node Tm
TUNE_TM_PARAM(nodeTmBase, 153, 100, 300, 10, 0.002)
TUNE_TM_PARAM(nodeTmMultiplier, 174, 80, 250, 8, 0.002)

// Search
TUNE_PARAM(rfpDepthMargin, 75, 40, 200, 10, 0.002)
TUNE_PARAM(rfpImprovingMargin, 61, 40, 200, 10, 0.002)
TUNE_PARAM(rfpIIRMargin, 76, 40, 200, 10, 0.002)
TUNE_PARAM(nmpReductionEvalDivisor, 221, 100, 400, 20, 0.002)
TUNE_PARAM(nmpDepthMargin, 28, 20, 40, 2, 0.002)
TUNE_PARAM(nmpOffset, 204, -300, 300, 20, 0.002)
TUNE_PARAM(razoringCoeff, 258, 100, 400, 20, 0.002)
TUNE_PARAM(probcutBaseMargin, 287, 100, 400, 10, 0.002)
TUNE_PARAM(probcutImprovingOffset, 54, -150, 150, 10, 0.002)
TUNE_PARAM(historyQuietLmrDivisor, 8049, 1, 16383, 100, 0.002)
TUNE_PARAM(historyNoisyLmrDivisor, 5922, 1, 16383, 100, 0.002)
TUNE_PARAM(doDeeperBaseMargin, 77, 1, 200, 20, 0.002)
TUNE_PARAM(qsBaseFutility, 268, -500, 500, 25, 0.002)
TUNE_PARAM(hindsightEval, 155, 50, 300, 10, 0.002)
// HH
TUNE_PARAM(historyBonusMul, 333, 1, 1500, 32, 0.002)
TUNE_PARAM(historyBonusOffset, 159, -1024, 1024, 32, 0.002)
TUNE_PARAM(historyBonusMax, 2910, 1, 4096, 256, 0.002)
TUNE_PARAM(historyMalusMul, 398, 1, 1500, 32, 0.002)
TUNE_PARAM(historyMalusOffset, 139, -1024, 1024, 32, 0.002)
TUNE_PARAM(historyMalusMax, 452, 1, 4096, 256, 0.002)
// Capthist
TUNE_PARAM(capthistoryBonusMul, 349, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryBonusOffset, -76, -1024, 1024, 32, 0.002)
TUNE_PARAM(capthistoryBonusMax, 2296, 1, 4096, 256, 0.002)
TUNE_PARAM(capthistoryMalusMul, 310, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryMalusOffset, -88, -1024, 1024, 32, 0.002)
TUNE_PARAM(capthistoryMalusMax, 1306, 1, 4096, 256, 0.002)
// Conthist
TUNE_PARAM(conthistoryBonusMul, 159, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryBonusOffset, -135, -1024, 1024, 32, 0.002)
TUNE_PARAM(conthistoryBonusMax, 2538, 1, 4096, 256, 0.002)
TUNE_PARAM(conthistoryMalusMul, 401, 1, 1500, 16, 0.002)
TUNE_PARAM(conthistoryMalusOffset, 114, -1024, 1024, 32, 0.002)
TUNE_PARAM(conthistoryMalusMax, 806, 1, 4096, 256, 0.002)
TUNE_PARAM(conthistoryTTMalusMul, 155, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryTTMalusMax, 385, 1, 2048, 32, 0.002)
// Corrhist
TUNE_PARAM(corrhistoryPawnWeight, 29, 1, 256, 3, 0.002)
TUNE_PARAM(corrhistoryNonPawnWeight, 34, 1, 256, 3, 0.002)
TUNE_PARAM(contCorrthistoryWeight, 26, 1, 256, 3, 0.002)

// Roothist
TUNE_PARAM(roothistoryBonusMul, 225, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryBonusOffset, 165, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryBonusMax, 1780, 1, 4096, 256, 0.002)
TUNE_PARAM(roothistoryMalusMul, 402, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryMalusOffset, 75, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryMalusMax, 892, 1, 4096, 256, 0.002)
// LMR
TUNE_PARAM(lmrQuietBase, 107, 40, 150, 7, 0.002)
TUNE_PARAM(lmrQuietDivisor, 227, 150, 500, 15, 0.002)
TUNE_PARAM(lmrNoisyBase, -36, -70, 100, 5, 0.002)
TUNE_PARAM(lmrNoisytDivisor, 247, 150, 500, 15, 0.002)
// Forward pruning
TUNE_PARAM(lmrDepthDivisor, 8300, 1, 16383, 100, 0.002)
TUNE_PARAM(seeQuietMargin, -98, -150, -20, 5, 0.002)
TUNE_PARAM(seeNoisyMargin, -27, -100, -1, 3, 0.002)
TUNE_PARAM(futilityCoeff0, 232, 40, 300, 10, 0.002)
TUNE_PARAM(futilityCoeff1, 118, 40, 200, 10, 0.002)
TUNE_PARAM(histPruningMargin, -3753, -16384, 0, 256, 0.002)

