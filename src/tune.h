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
TUNE_PARAM(rfpDepthMargin, 74, 40, 200, 10, 0.002)
TUNE_PARAM(rfpImprovingMargin, 53, 40, 200, 10, 0.002)
TUNE_PARAM(rfpIIRMargin, 85, 40, 200, 10, 0.002)
TUNE_PARAM(nmpReductionEvalDivisor, 184, 100, 400, 20, 0.002)
TUNE_PARAM(nmpDepthMargin, 28, 20, 40, 2, 0.002)
TUNE_PARAM(nmpOffset, 207, -300, 300, 20, 0.002)
TUNE_PARAM(razoringCoeff, 260, 100, 400, 20, 0.002)
TUNE_PARAM(probcutBaseMargin, 273, 100, 400, 20, 0.002)
TUNE_PARAM(probcutImprovingOffset, 63, -150, 150, 10, 0.002)
TUNE_PARAM(historyQuietLmrDivisor, 8091, 1, 16383, 100, 0.002)
TUNE_PARAM(historyNoisyLmrDivisor, 6009, 1, 16383, 100, 0.002)
TUNE_PARAM(doDeeperBaseMargin, 103, 1, 200, 20, 0.002)
TUNE_PARAM(qsBaseFutility, 269, -500, 500, 25, 0.002)
TUNE_PARAM(hindsightEval, 143, 50, 300, 10, 0.002)
// Singular ext
TUNE_PARAM(doubleExtMargin, 10, 1, 50, 2, 0.002)
TUNE_PARAM(tripleExtMargin, 77, 30, 150, 10, 0.002)
// HH
TUNE_PARAM(historyBonusMul, 339, 1, 1500, 32, 0.002)
TUNE_PARAM(historyBonusOffset, 41, -1024, 1024, 64, 0.002)
TUNE_PARAM(historyBonusMax, 2605, 1, 4096, 256, 0.002)
TUNE_PARAM(historyMalusMul, 365, 1, 1500, 32, 0.002)
TUNE_PARAM(historyMalusOffset, 29, -1024, 1024, 64, 0.002)
TUNE_PARAM(historyMalusMax, 651, 1, 4096, 256, 0.002)
// Capthist
TUNE_PARAM(capthistoryBonusMul, 321, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryBonusOffset, -99, -1024, 1024, 64, 0.002)
TUNE_PARAM(capthistoryBonusMax, 2018, 1, 4096, 256, 0.002)
TUNE_PARAM(capthistoryMalusMul, 302, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryMalusOffset, -8, -1024, 1024, 64, 0.002)
TUNE_PARAM(capthistoryMalusMax, 1466, 1, 4096, 256, 0.002)
// Conthist
TUNE_PARAM(conthistoryBonusMul, 143, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryBonusOffset, -174, -1024, 1024, 64, 0.002)
TUNE_PARAM(conthistoryBonusMax, 2684, 1, 4096, 256, 0.002)
TUNE_PARAM(conthistoryMalusMul, 233, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryMalusOffset, 23, -1024, 1024, 64, 0.002)
TUNE_PARAM(conthistoryMalusMax, 1546, 1, 4096, 256, 0.002)
// Roothist
TUNE_PARAM(roothistoryBonusMul, 184, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryBonusOffset, 119, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryBonusMax, 2289, 1, 4096, 256, 0.002)
TUNE_PARAM(roothistoryMalusMul, 380, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryMalusOffset, 92, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryMalusMax, 854, 1, 4096, 256, 0.002)
// LMR
TUNE_PARAM(lmrQuietBase, 105, 40, 150, 7, 0.002)
TUNE_PARAM(lmrQuietDivisor, 224, 150, 500, 15, 0.002)
TUNE_PARAM(lmrNoisyBase, -43, -70, 100, 7, 0.002)
TUNE_PARAM(lmrNoisytDivisor, 249, 150, 500, 15, 0.002)
// Forward pruning
TUNE_PARAM(lmrDepthDivisor, 8199, 1, 16383, 100, 0.002)
TUNE_PARAM(seeQuietMargin, -99, -150, -20, 5, 0.002)
TUNE_PARAM(seeNoisyMargin, -24, -100, -1, 3, 0.002)
TUNE_PARAM(futilityCoeff0, 241, 40, 300, 10, 0.002)
TUNE_PARAM(futilityCoeff1, 111, 40, 200, 10, 0.002)
TUNE_PARAM(histPruningMargin, -3889, -16384, 0, 256, 0.002)

