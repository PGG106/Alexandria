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
TUNE_PARAM(optTimeMultiplier, 75, 50, 90, 2, 0.002)
TUNE_PARAM(maxTimeMultiplier, 315, 100, 500, 20, 0.002)

// Bestmove stability
TUNE_PARAM(bmScale1, 243, 50, 300, 10, 0.002)
TUNE_PARAM(bmScale2, 139, 50, 200, 10, 0.002)
TUNE_PARAM(bmScale3, 108, 50, 150, 6, 0.002)
TUNE_PARAM(bmScale4, 86, 40, 110, 5, 0.002)
TUNE_PARAM(bmScale5, 71, 35, 100, 5, 0.002)

// Eval stability
TUNE_PARAM(evalScale1, 128, 90, 160, 4, 0.002)
TUNE_PARAM(evalScale2, 115, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale3, 101, 80, 150, 4, 0.002)
TUNE_PARAM(evalScale4, 92, 60, 130, 4, 0.002)
TUNE_PARAM(evalScale5, 88, 40, 110, 4, 0.002)

// Node Tm
TUNE_PARAM(nodeTmBase, 146, 100, 300, 10, 0.002)
TUNE_PARAM(nodeTmMultiplier, 171, 80, 250, 8, 0.002)


// Search Parameters

// Aspiration Windows
TUNE_PARAM(aspDelta, 12, 1, 200, 0.5, 0.002)
TUNE_PARAM(deltaMultiplier, 143, 101, 400, 15, 0.002)

// Static evaluation history bonus
TUNE_PARAM(sevalBonusScale, -10, -30, -1, 0.5, 0.002)
TUNE_PARAM(sevalMinClamp, -1762, -10000, -1, 150, 0.002)
TUNE_PARAM(sevalMaxClamp, 1487, 1, 10000, 150, 0.002)
TUNE_PARAM(sevalTempo, 630, -1000, 3000, 69, 0.002)

// Complexity
TUNE_PARAM(complexityScale, 95, 0, 1000, 20, 0.002)

// RFP
TUNE_PARAM(rfpMarginScale, 92, 1, 180, 8, 0.002)
TUNE_PARAM(rfpImprovingScale, 90, 1, 180, 12, 0.002)
TUNE_PARAM(rfpIIRScale, 94, 1, 180, 12, 0.002)

// NMP
TUNE_PARAM(nmpSevalDepthMultiplier, 30, -100, 100, 3, 0.002)
TUNE_PARAM(nmpSevalDepthConstant, 175, -2000, 2000, 17, 0.002)
TUNE_PARAM(nmpEvalBetaDivisor, 198, 20, 3000, 15, 0.002)
TUNE_PARAM(nmpEvalBetaCap, 4, 3, 20, 3, 0.002)

// Razoring
TUNE_PARAM(razoringMultiplier, 243, 10, 4000, 30, 0.002)

// Probcut
TUNE_PARAM(probcutMargin, 298, 1, 7000, 30, 0.002)
TUNE_PARAM(probcutImproving, 54, -100, 100, 8, 0.002)

// LMR (Moves Loop Pruning)
TUNE_PARAM(lmrCaptureReductionBase, -238, -10000, 10000, 100, 0.002)
TUNE_PARAM(lmrCaptureReductionDivisor, 2258, 2, 10000, 240, 0.002)
TUNE_PARAM(lmrQuietReductionBase, 986, -10000, 10000, 100, 0.002)
TUNE_PARAM(lmrQuietReductionDivisor, 1926, 2, 10000, 200, 0.002)
TUNE_PARAM(lmrDepthHistoryDivsior, 8149, 1, 1111111, 345, 0.002)

// LMP
TUNE_PARAM(NotImprovingLMPMarginConstant, 1487, 0, 30000, 300, 0.002)
TUNE_PARAM(NotImprovingLMPMarginMultiplier, 424, 0, 30000, 80, 0.002)
TUNE_PARAM(NotImprovingLMPMarginExponent, 1956, 0, 30000, 100, 0.002)
TUNE_PARAM(ImprovingLMPMarginConstant, 3127, 0, 30000, 300, 0.002)
TUNE_PARAM(ImprovingLMPMarginMultiplier, 1052, 0, 30000, 100, 0.002)
TUNE_PARAM(ImprovingLMPMarginExponent, 2064, 0, 30000, 100, 0.002)

// Futility Pruning
TUNE_PARAM(futilityConstant, 234, 0, 2025, 20, 0.002)
TUNE_PARAM(futilityLMRDepthMultiplier, 147, 0, 2024, 13, 0.002)

// SEE Pruning
TUNE_PARAM(SEEQuietMultiplier, -8072, -1001010, 0, 500, 0.002)
TUNE_PARAM(SEEQuietExponent, 1014, 500, 3000, 35, 0.002)
TUNE_PARAM(SEENonQuietMultiplier, -2887, -1001010, 0, 200, 0.002)
TUNE_PARAM(SEENonQuietExponent, 1990, 500, 3000, 35, 0.002)

// Singular Extensions
TUNE_PARAM(SEDoubleExtensionsMargin, 17, 0, 1000, 3, 0.002)
TUNE_PARAM(SEDoubleExtensionsLimit, 7, 7, 100, 4, 0.002)
TUNE_PARAM(SETripleExtensionsMargin, 99, 0, 1000, 10, 0.002)

// LMR
TUNE_PARAM(QuietHistoryDivisor, 8071, 1, 10101010, 368, 0.002)
TUNE_PARAM(CaptureHistoryDivisor, 6186, 1, 1010101, 368, 0.002)

// Full-Window Search Tweaks
TUNE_PARAM(DDSBase, 52, -1000, 1000, 5, 0.002)

// QS Futility
TUNE_PARAM(QSFutilityMargin, 190, 0, 20202, 12, 0.002)

// History Bonus
TUNE_PARAM(HistBonusQuadraticCoefficient, 16, 0, 1000, 0.5, 0.002)
TUNE_PARAM(HistBonusLinearCoefficient, 32, 0, 1000, 1.5, 0.002)
TUNE_PARAM(HistBonusConstant, 16, 0, 1000, 0.5, 0.002)
TUNE_PARAM(HistBonusClamp, 1187, 0, 8192, 80, 0.002)

// Correction History
TUNE_PARAM(CorrHistClamp, 128, 0, 1024, 8, 0.002)

// History Max
TUNE_PARAM(HH_MAX, 8482, 1, 65536, 400, 0.002)
TUNE_PARAM(RH_MAX, 8267, 1, 65536, 400, 0.002)
TUNE_PARAM(CH_MAX, 16092, 1, 65536, 800, 0.002)
TUNE_PARAM(CAPTHIST_MAX, 16654, 1, 65536, 800, 0.002)

// History Aggregate
TUNE_PARAM(MainHistoryScale, 131, 0, 2048, 14, 0.002)
TUNE_PARAM(ContinuationHistoryScale, 125, 0, 2048, 14, 0.002)
TUNE_PARAM(RootHistoryScale, 491, 0, 2048, 50, 0.002)

// MVV Multiplier
TUNE_PARAM(MVVMultiplier, 258, 0, 19100, 8, 0.002)

// Bad Capture Threshold
TUNE_PARAM(SEEThresholdConstant, 233, -1010, 1101, 15, 0.002)
TUNE_PARAM(SEEThresholdDivisor, 32, -1010, 1101, 1, 0.002)

// Evaluation Parameters

TUNE_PARAM(knightPhaseValue, 411, -1000, 10000, 28, 0.002)
TUNE_PARAM(bishopPhaseValue, 396, -1000, 10000, 28, 0.002)
TUNE_PARAM(rookPhaseValue, 612, -1000, 10000, 56, 0.002)
TUNE_PARAM(queenPhaseValue, 1230, -1000, 10000, 112, 0.002)
TUNE_PARAM(maxPhaseValue, 8333, 100, 10000, 400, 0.002)
TUNE_PARAM(materialScalingBase, 24387, -1000, 50000, 1372, 0.002)
