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
TUNE_PARAM(baseMultiplier, 59, 20, 150, 7, 0.002)
TUNE_PARAM(incMultiplier, 84, 50, 150, 5, 0.002)
TUNE_PARAM(maxBoundMultiplier, 73, 50, 90, 2, 0.002)
TUNE_PARAM(optTimeMultiplier, 79, 50, 90, 2, 0.002)
TUNE_PARAM(maxTimeMultiplier, 284, 100, 500, 20, 0.002)

// Bestmove stability
TUNE_PARAM(bmScale1, 247, 50, 300, 10, 0.002)
TUNE_PARAM(bmScale2, 147, 50, 200, 10, 0.002)
TUNE_PARAM(bmScale3, 116, 50, 150, 6, 0.002)
TUNE_PARAM(bmScale4, 90, 40, 110, 5, 0.002)
TUNE_PARAM(bmScale5, 65, 35, 100, 5, 0.002)

// Eval stability
TUNE_PARAM(evalScale1, 120, 90, 160, 4, 0.002)
TUNE_PARAM(evalScale2, 118, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale3, 103, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale4, 100, 60, 130, 4, 0.002)
TUNE_PARAM(evalScale5, 83, 40, 110, 4, 0.002)

// Node Tm
TUNE_PARAM(nodeTmBase, 152, 100, 300, 10, 0.002)
TUNE_PARAM(nodeTmMultiplier, 183, 80, 250, 8, 0.002)


// Search
TUNE_PARAM(rfpDepthMargin, 79, 40, 200, 10, 0.002)
TUNE_PARAM(rfpImprovingMargin, 79, 40, 200, 10, 0.002)
TUNE_PARAM(rfpIIRMargin, 79, 40, 200, 10, 0.002)
TUNE_PARAM(nmpReductionEvalDivisor, 195, 100, 400, 20, 0.002)
TUNE_PARAM(razoringCoeff, 243, 100, 400, 20, 0.002)
TUNE_PARAM(historyQuietLmrDivisor, 8177, 1, 16383, 100, 0.002)
TUNE_PARAM(historyNoisyLmrDivisor, 5941, 1, 16383, 100, 0.002)
TUNE_PARAM(doDeeperBaseMargin, 117, 1, 200, 20, 0.002)
TUNE_PARAM(qsBaseFutility, 295, -500, 500, 25, 0.002)
TUNE_PARAM(historyBonusMax, 1286, 1, 4096, 256, 0.002)
TUNE_PARAM(historyMalusMax, 1286, 1, 4096, 256, 0.002)
TUNE_PARAM(lmrQuietBase, 106, 40, 150, 7, 0.002)
TUNE_PARAM(lmrQuietDivisor, 229, 150, 500, 15, 0.002)
TUNE_PARAM(lmrNoisyBase, -27, -70, 100, 7, 0.002)
TUNE_PARAM(lmrNoisytDivisor, 234, 150, 500, 15, 0.002)
TUNE_PARAM(seeQuietMargin, -93, -150, -20, 5, 0.002)
TUNE_PARAM(seeNoisyMargin, -34, -100, -1, 3, 0.002)
TUNE_PARAM(futilityCoeff0, 242, 40, 300, 10, 0.002)
TUNE_PARAM(futilityCoeff1, 137, 40, 200, 10, 0.002)
TUNE_PARAM(lmrDepthDivisor, 8135, 1, 16383, 100, 0.002)
