#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>

#define TUNE

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
// Actual functions to init and update variables
inline const int &addTune(std::string name, int defaultValue, int curr_value, int min_value, int max_value, float C_end, float R_end)
{
    tunable_param &param = tunableParams()[name];
    param = tunable_param{name, defaultValue, curr_value, min_value, max_value, C_end, R_end};
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
TUNE_PARAM(baseMultiplier, 54, 20, 150, 7, 0.002)
TUNE_PARAM(incMultiplier, 85, 50, 150, 5, 0.002)
TUNE_PARAM(maxBoundMultiplier, 76, 50, 90, 2, 0.002)
TUNE_PARAM(optTimeMultiplier, 76, 50, 90, 2, 0.002)
TUNE_PARAM(maxTimeMultiplier, 304, 100, 500, 20, 0.002)

// Bestmove stability
TUNE_PARAM(bmScale1, 243, 50, 300, 10, 0.002)
TUNE_PARAM(bmScale2, 135, 50, 200, 10, 0.002)
TUNE_PARAM(bmScale3, 109, 50, 150, 6, 0.002)
TUNE_PARAM(bmScale4, 88, 40, 110, 5, 0.002)
TUNE_PARAM(bmScale5, 68, 35, 100, 5, 0.002)

// Eval stability
TUNE_PARAM(stabilityFactor, 10, 5, 15, 1, 0.002)
TUNE_PARAM(evalScale1, 125, 90, 160, 4, 0.002)
TUNE_PARAM(evalScale2, 115, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale3, 100, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale4, 94, 60, 130, 4, 0.002)
TUNE_PARAM(evalScale5, 88, 40, 110, 4, 0.002)

// Node Tm
TUNE_PARAM(nodeTmBase, 152, 100, 300, 10, 0.002)
TUNE_PARAM(nodeTmMultiplier, 174, 80, 250, 8, 0.002)

// SEARCH STUFF

// ASP WINDOWS
TUNE_PARAM(aspWinDelta, 12, 6, 18, 1, 0.002)
TUNE_PARAM(aspWinDepth, 3, 2, 7, 1, 0.002)
TUNE_PARAM(aspWinDeltaResize, 144, 112, 151, 3, 0.002)

// RFP
TUNE_PARAM(rfpRed, 91, 50, 150, 5, 0.002)

//NMP
TUNE_PARAM(nmpStaticevalRedDepth, 30, 10, 50, 2, 0.002)
TUNE_PARAM(nmpStaticevalRedFixed, 170, 100, 250, 8, 0.002)
TUNE_PARAM(nmpDepthRedRatio, 200, 100, 300, 10, 0.002)

// Razoring
TUNE_PARAM(rzrDepthMul, 256, 150, 325, 10, 0.002)

// LMR DEPTH
TUNE_PARAM(lmrDepthHistDiv, 8192, 4096, 16384, 250, 0.002)

// FP
TUNE_PARAM(fpFixed, 250, 150, 325, 8, 0.002)
TUNE_PARAM(fpScale, 150, 75, 210, 7, 0.002)

// LMR
TUNE_PARAM(lmrComplexity, 50, 25, 150, 5, 0.002)
TUNE_PARAM(lmrQuietHistDiv, 8192, 4096, 16384, 250, 0.002)
TUNE_PARAM(lmrNoisyHistDiv, 6144, 4096, 16384, 250, 0.002)

//DO-DEEPER
TUNE_PARAM(DDFixed, 53, 35, 85, 4, 0.002)

// QS STUFF
TUNE_PARAM(QSFpFixed, 192, 120, 250, 7, 0.002)
TUNE_PARAM(QSFpSEE, 1, -1, 100, 5, 0.002)
