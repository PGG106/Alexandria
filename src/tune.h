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
TUNE_PARAM(evalScale1, 125, 90, 160, 4, 0.002)
TUNE_PARAM(evalScale2, 115, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale3, 100, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale4, 94, 60, 130, 4, 0.002)
TUNE_PARAM(evalScale5, 88, 40, 110, 4, 0.002)

// Node Tm
TUNE_PARAM(nodeTmBase, 152, 100, 300, 10, 0.002)
TUNE_PARAM(nodeTmMultiplier, 174, 80, 250, 8, 0.002)


// Search Parameters

// Aspiration Windows
TUNE_PARAM(aspDelta, 12, 1, 200, 0.5, 0.002)
TUNE_PARAM(deltaMultiplier, 144, 101, 400, 15, 0.002)

// Static evaluation history bonus
TUNE_PARAM(sevalBonusScale, -10, -30, -1, 0.5, 0.002)
TUNE_PARAM(sevalMinClamp, -1830, -10000, -1, 150, 0.002)
TUNE_PARAM(sevalMaxClamp, 1427, 1, 10000, 150, 0.002)
TUNE_PARAM(sevalTempo, 624, -1000, 3000, 69, 0.002)

// Complexity
TUNE_PARAM(complexityScale, 100, 0, 1000, 20, 0.002)

// RFP
TUNE_PARAM(rfpMarginScale, 91, 1, 180, 8, 0.002)

// NMP
TUNE_PARAM(nmpSevalDepthMultiplier, 30, -100, 100, 3, 0.002)
TUNE_PARAM(nmpSevalDepthConstant, 170, -2000, 2000, 17, 0.002)
TUNE_PARAM(nmpEvalBetaDivisor, 200, 20, 3000, 15, 0.002)
TUNE_PARAM(nmpEvalBetaCap, 3, 3, 20, 3, 0.002)

// Razoring
TUNE_PARAM(razoringMultiplier, 256, 10, 4000, 30, 0.002)

// Probcut
TUNE_PARAM(probcutMargin, 300, 1, 7000, 30, 0.002)
TUNE_PARAM(probcutImproving, 50, -100, 100, 8, 0.002)

// LMR (Moves Loop Pruning)
TUNE_PARAM(lmrCaptureReductionBase, -250, -10000, 10000, 100, 0.002)
TUNE_PARAM(lmrCaptureReductionDivisor, 2250, 2, 10000, 240, 0.002)
TUNE_PARAM(lmrQuietReductionBase, 1000, -10000, 10000, 100, 0.002)
TUNE_PARAM(lmrQuietReductionDivisor, 2000, 2, 10000, 200, 0.002)
TUNE_PARAM(lmrDepthHistoryDivsior, 8192, 1, 1111111, 345, 0.002)

// LMP
TUNE_PARAM(NotImprovingLMPMarginConstant, 1500, 0, 30000, 300, 0.002)
TUNE_PARAM(NotImprovingLMPMarginMultiplier, 500, 0, 30000, 80, 0.002)
TUNE_PARAM(NotImprovingLMPMarginExponent, 2000, 0, 30000, 100, 0.002)
TUNE_PARAM(ImprovingLMPMarginConstant, 3000, 0, 30000, 300, 0.002)
TUNE_PARAM(ImprovingLMPMarginMultiplier, 1000, 0, 30000, 100, 0.002)
TUNE_PARAM(ImprovingLMPMarginExponent, 2000, 0, 30000, 100, 0.002)

// Futility Pruning
TUNE_PARAM(futilityConstant, 250, 0, 2025, 20, 0.002)
TUNE_PARAM(futilityLMRDepthMultiplier, 150, 0, 2024, 13, 0.002)

// SEE Pruning
TUNE_PARAM(SEEQuietMultiplier, -8000, -1001010, 0, 500, 0.002)
TUNE_PARAM(SEEQuietExponent, 1000, 500, 3000, 35, 0.002)
TUNE_PARAM(SEENonQuietMultiplier, -3000, -1001010, 0, 200, 0.002)
TUNE_PARAM(SEENonQuietExponent, 2000, 500, 3000, 35, 0.002)

// Singular Extensions
TUNE_PARAM(SEDoubleExtensionsMargin, 17, 0, 1000, 3, 0.002)
TUNE_PARAM(SEDoubleExtensionsLimit, 11, 7, 100, 4, 0.002)
TUNE_PARAM(SETripleExtensionsMargin, 100, 0, 1000, 10, 0.002)

// LMR
TUNE_PARAM(QuietHistoryDivisor, 8192, 1, 10101010, 368, 0.002)
TUNE_PARAM(CaptureHistoryDivisor, 6144, 1, 1010101, 368, 0.002)

// Full-Window Search Tweaks
TUNE_PARAM(DDSBase, 53, -1000, 1000, 5, 0.002)

// QS Futility
TUNE_PARAM(QSFutilityMargin, 192, 0, 20202, 12, 0.002)

// History Bonus
TUNE_PARAM(HistBonusQuadraticCoefficient, 16, 0, 1000, 0.5, 0.002)
TUNE_PARAM(HistBonusLinearCoefficient, 32, 0, 1000, 1.5, 0.002)
TUNE_PARAM(HistBonusConstant, 16, 0, 1000, 0.5, 0.002)
TUNE_PARAM(HistBonusClamp, 1200, 0, 8192, 80, 0.002)

// Correction History
TUNE_PARAM(CorrHistClamp, 128, 0, 1024, 8, 0.002)

// History Aggregate
TUNE_PARAM(MainHistoryScale, 128, 0, 2048, 14, 0.002)
TUNE_PARAM(ContinuationHistoryScale, 128, 0, 2048, 14, 0.002)
TUNE_PARAM(RootHistoryScale, 512, 0, 2048, 50, 0.002)

// MVV Multiplier
TUNE_PARAM(MVVMultiplier, 256, 0, 19100, 8, 0.002)

// Bad Capture Threshold
TUNE_PARAM(SEEThresholdConstant, 236, -1010, 1101, 15, 0.002)
TUNE_PARAM(SEEThresholdDivisor, 32, -1010, 1101, 1, 0.002)
