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
TUNE_PARAM(rfpDepthMargin, 81, 40, 200, 10, 0.002)
TUNE_PARAM(rfpImprovingMargin, 61, 40, 200, 10, 0.002)
TUNE_PARAM(rfpIIRMargin, 78, 40, 200, 10, 0.002)
TUNE_PARAM(nmpReductionEvalDivisor, 202, 100, 400, 20, 0.002)
TUNE_PARAM(nmpDepthMargin, 29, 20, 40, 2, 0.002)
TUNE_PARAM(nmpOffset, 192, -300, 300, 20, 0.002)
TUNE_PARAM(razoringCoeff, 247, 100, 400, 20, 0.002)
TUNE_PARAM(probcutBaseMargin, 288, 100, 400, 20, 0.002)
TUNE_PARAM(probcutImprovingOffset, 55, -150, 150, 10, 0.002)
TUNE_PARAM(historyQuietLmrDivisor, 8134, 1, 16383, 100, 0.002)
TUNE_PARAM(historyNoisyLmrDivisor, 5873, 1, 16383, 100, 0.002)
TUNE_PARAM(doDeeperBaseMargin, 88, 1, 200, 20, 0.002)
TUNE_PARAM(qsBaseFutility, 282, -500, 500, 25, 0.002)
TUNE_PARAM(hindsightEval, 150, 50, 300, 10, 0.002)
// HH
TUNE_PARAM(historyBonusMul, 313, 1, 1500, 32, 0.002)
TUNE_PARAM(historyBonusOffset, 21, -1024, 1024, 64, 0.002)
TUNE_PARAM(historyBonusMax, 2663, 1, 4096, 256, 0.002)
TUNE_PARAM(historyMalusMul, 377, 1, 1500, 32, 0.002)
TUNE_PARAM(historyMalusOffset, 50, -1024, 1024, 64, 0.002)
TUNE_PARAM(historyMalusMax, 865, 1, 4096, 256, 0.002)
// Capthist
TUNE_PARAM(capthistoryBonusMul, 336, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryBonusOffset, -89, -1024, 1024, 64, 0.002)
TUNE_PARAM(capthistoryBonusMax, 2071, 1, 4096, 256, 0.002)
TUNE_PARAM(capthistoryMalusMul, 333, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryMalusOffset, -84, -1024, 1024, 64, 0.002)
TUNE_PARAM(capthistoryMalusMax, 1553, 1, 4096, 256, 0.002)
// Conthist
TUNE_PARAM(conthistoryBonusMul, 149, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryBonusOffset, -123, -1024, 1024, 64, 0.002)
TUNE_PARAM(conthistoryBonusMax, 2587, 1, 4096, 256, 0.002)
TUNE_PARAM(conthistoryMalusMul, 388, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryMalusOffset, 62, -1024, 1024, 64, 0.002)
TUNE_PARAM(conthistoryMalusMax, 891, 1, 4096, 256, 0.002)
TUNE_PARAM(conthistoryTTMalusMul, 100, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryTTMalusMax, 400, 1, 2048, 64, 0.002)

// Roothist
TUNE_PARAM(roothistoryBonusMul, 218, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryBonusOffset, 115, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryBonusMax, 2103, 1, 4096, 256, 0.002)
TUNE_PARAM(roothistoryMalusMul, 388, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryMalusOffset, 62, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryMalusMax, 891, 1, 4096, 256, 0.002)
// LMR
TUNE_PARAM(lmrQuietBase, 102, 40, 150, 7, 0.002)
TUNE_PARAM(lmrQuietDivisor, 222, 150, 500, 15, 0.002)
TUNE_PARAM(lmrNoisyBase, -41, -70, 100, 7, 0.002)
TUNE_PARAM(lmrNoisytDivisor, 234, 150, 500, 15, 0.002)
// Forward pruning
TUNE_PARAM(lmrDepthDivisor, 8285, 1, 16383, 100, 0.002)
TUNE_PARAM(seeQuietMargin, -95, -150, -20, 5, 0.002)
TUNE_PARAM(seeNoisyMargin, -70, -100, -1, 3, 0.002)
TUNE_PARAM(futilityCoeff0, 250, 40, 300, 10, 0.002)
TUNE_PARAM(futilityCoeff1, 120, 40, 200, 10, 0.002)
TUNE_PARAM(histPruningMargin, -4000, -16384, 0, 256, 0.002)

