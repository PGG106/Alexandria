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

// Score-loss TM scaling
TUNE_TM_PARAM(scoreTmBase, 86, 0, 150, 8, 0.002)
TUNE_TM_PARAM(scoreTmIDPrev, 10, 0, 20, 2, 0.002)
TUNE_TM_PARAM(scoreTmPrevSearch, 25, 0, 50, 3, 0.002)
TUNE_TM_PARAM(scoreTmClampMin, 81, 0, 150, 6, 0.002)
TUNE_TM_PARAM(scoreTmClampMax, 150, 75, 225, 8, 0.002)

// Search
TUNE_PARAM(aspWinDelta, 9, 2, 15, 2, 0.002)
TUNE_PARAM(aspWinPrevevalDiv, 18654, 12000, 25000, 512, 0.002)
TUNE_PARAM(evalHistSlope, -11, -30, 10, 2, 0.002)
TUNE_PARAM(evalHistMin, -1847, -3000, 0, 64, 0.002)
TUNE_PARAM(evalHistMax, 1480, 0, 3000, 64, 0.002)
TUNE_PARAM(evalHistOffset, 633, -1000, 2000, 32, 0.002)
TUNE_PARAM(rfpDepthMargin, 79, 40, 200, 10, 0.002)
TUNE_PARAM(rfpImprovingMargin, 58, 40, 200, 10, 0.002)
TUNE_PARAM(rfpIIRMargin, 72, 40, 200, 10, 0.002)
TUNE_PARAM(rfpMinMargin, -97, -300, 200, 8, 0.002)
TUNE_PARAM(nmpReductionEvalDivisor, 208, 100, 400, 20, 0.002)
TUNE_PARAM(nmpDepthMargin, 28, 20, 40, 2, 0.002)
TUNE_PARAM(nmpOffset, 218, -300, 300, 20, 0.002)
TUNE_PARAM(razoringCoeff, 324, 100, 400, 20, 0.002)
TUNE_PARAM(probcutBaseMargin, 286, 100, 400, 20, 0.002)
TUNE_PARAM(probcutImprovingOffset, 52, -150, 150, 10, 0.002)
TUNE_PARAM(historyQuietLmrDivisor, 8189, 1, 16383, 100, 0.002)
TUNE_PARAM(historyNoisyLmrDivisor, 5916, 1, 16383, 100, 0.002)
TUNE_PARAM(doDeeperBaseMargin, 70, 1, 200, 20, 0.002)
TUNE_PARAM(doShallowerBaseMargin, 0, -100, 100, 8, 0.002)
TUNE_PARAM(hindsightEval, 147, 50, 300, 10, 0.002)
TUNE_PARAM(qsBaseFutility, 274, -500, 500, 25, 0.002)
TUNE_PARAM(qsSEEmargin, -43, -150, 1, 10, 0.002)
// HH
TUNE_PARAM(historyBonusMul, 340, 1, 1500, 32, 0.002)
TUNE_PARAM(historyBonusOffset, 124, -1024, 1024, 64, 0.002)
TUNE_PARAM(historyBonusMax, 2909, 1, 4096, 256, 0.002)
TUNE_PARAM(historyMalusMul, 402, 1, 1500, 32, 0.002)
TUNE_PARAM(historyMalusOffset, 121, -1024, 1024, 64, 0.002)
TUNE_PARAM(historyMalusMax, 339, 1, 4096, 256, 0.002)
// Capthist
TUNE_PARAM(capthistoryBonusMul, 372, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryBonusOffset, -132, -1024, 1024, 64, 0.002)
TUNE_PARAM(capthistoryBonusMax, 2203, 1, 4096, 256, 0.002)
TUNE_PARAM(capthistoryMalusMul, 333, 1, 1500, 32, 0.002)
TUNE_PARAM(capthistoryMalusOffset, -105, -1024, 1024, 64, 0.002)
TUNE_PARAM(capthistoryMalusMax, 1426, 1, 4096, 256, 0.002)
// Conthist
TUNE_PARAM(conthistoryBonusMul, 176, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryBonusOffset, -103, -1024, 1024, 64, 0.002)
TUNE_PARAM(conthistoryBonusMax, 2379, 1, 4096, 256, 0.002)
TUNE_PARAM(conthistoryMalusMul, 390, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryMalusOffset, 53, -1024, 1024, 64, 0.002)
TUNE_PARAM(conthistoryMalusMax, 816, 1, 4096, 256, 0.002)
TUNE_PARAM(conthistoryTTMalusMul, 170, 1, 1500, 32, 0.002)
TUNE_PARAM(conthistoryTTMalusMax, 421, 1, 2048, 64, 0.002)
// Pawnhist
TUNE_PARAM(pawnhistoryBonusMul, 174, 1, 1500, 32, 0.002)
TUNE_PARAM(pawnhistoryBonusOffset, -34, -1024, 1024, 64, 0.002)
TUNE_PARAM(pawnhistoryBonusMax, 2490, 1, 4096, 256, 0.002)
TUNE_PARAM(pawnhistoryMalusMul, 408, 1, 1500, 32, 0.002)
TUNE_PARAM(pawnhistoryMalusOffset, 69, -1024, 1024, 64, 0.002)
TUNE_PARAM(pawnhistoryMalusMax, 702, 1, 4096, 256, 0.002)
// Corrhist
TUNE_PARAM(corrhistoryPawnWeight, 24, 1, 256, 4, 0.002)
TUNE_PARAM(corrhistoryNonPawnWeight, 34, 1, 256, 4, 0.002)
TUNE_PARAM(contCorrthistoryWeight, 26, 1, 256, 4, 0.002)

// Roothist
TUNE_PARAM(roothistoryBonusMul, 240, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryBonusOffset, 161, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryBonusMax, 1472, 1, 4096, 256, 0.002)
TUNE_PARAM(roothistoryMalusMul, 398, 1, 1500, 32, 0.002)
TUNE_PARAM(roothistoryMalusOffset, 77, -1024, 1024, 64, 0.002)
TUNE_PARAM(roothistoryMalusMax, 685, 1, 4096, 256, 0.002)
// LMR
TUNE_PARAM(lmrQuietBase, 115, 40, 150, 7, 0.002)
TUNE_PARAM(lmrQuietDivisor, 218, 150, 500, 15, 0.002)
TUNE_PARAM(lmrNoisyBase, -34, -70, 100, 7, 0.002)
TUNE_PARAM(lmrNoisytDivisor, 249, 150, 500, 15, 0.002)
// Forward pruning
TUNE_PARAM(lmrDepthDivisor, 8222, 1, 16383, 100, 0.002)
TUNE_PARAM(seeQuietMargin, -96, -150, -20, 5, 0.002)
TUNE_PARAM(seeNoisyMargin, -30, -100, -1, 3, 0.002)
TUNE_PARAM(futilityCoeff0, 216, 40, 300, 10, 0.002)
TUNE_PARAM(futilityCoeff1, 116, 40, 200, 10, 0.002)
TUNE_PARAM(histPruningMargin, -3563, -16384, 0, 256, 0.002)

