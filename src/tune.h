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

TUNE_PARAM(minAspDepth, 4, 2, 6, 0.5, 0.002)
TUNE_PARAM(aspWindowStart, 10, 8, 15, 0.5, 0.002)
TUNE_PARAM(aspWindowWidenScale, 81, 65, 128, 3.0, 0.002)

TUNE_PARAM(maxImprovementPer256, 384, 256, 512, 13.0, 0.002)

TUNE_PARAM(rfpMaxDepth, 8, 4, 14, 0.5, 0.002)
TUNE_PARAM(rfpDepthCoeff, 70, 20, 200, 10.0, 0.002)
TUNE_PARAM(rfpImprCoeff, 70, 20, 200, 10.0, 0.002)

TUNE_PARAM(nmpDepth, 3, 1, 5, 0.5, 0.002)
TUNE_PARAM(nmpRedConst, 3072, 1024, 5120, 400.0, 0.002)
TUNE_PARAM(nmpRedDepthCoeff, 341, 170, 480, 34.0, 0.002)
TUNE_PARAM(nmpRedEvalDiffMax, 896, 384, 1280, 54.0, 0.002)
TUNE_PARAM(nmpRedEvalDiffDiv, 256, 128, 512, 20.0, 0.002)

TUNE_PARAM(lmpMarginConst, 300, 100, 500, 50.0, 0.002)
TUNE_PARAM(lmpMarginMult, 100, 60, 140, 7.0, 0.002)
TUNE_PARAM(lmpMarginPower, 200, 150, 250, 10.0, 0.002)

TUNE_PARAM(fpMarginQuadratic, 16, 0, 32, 2.0, 0.002)
TUNE_PARAM(fpMarginLinear, 13, 0, 26, 2.0, 0.002)
TUNE_PARAM(fpMarginConst, 27, 0, 54, 10.0, 0.002)
TUNE_PARAM(fpDepth, 10, 5, 15, 0.5, 0.002)

TUNE_PARAM(tacticalSeeCoeff, 3000, 1000, 8000, 350.0, 0.002)
TUNE_PARAM(tacticalSeePower, 200, 50, 400, 15.0, 0.002)
TUNE_PARAM(quietSeeCoeff, 8000, 1000, 16000, 750.0, 0.002)
TUNE_PARAM(quietSeePower, 100, 50, 300, 13.0, 0.002)

TUNE_PARAM(seDepth, 5, 3, 7, 0.5, 0.002)
TUNE_PARAM(seMinQuality, 3, 3, 4, 0.5, 0.002)
TUNE_PARAM(seMarginMult, 48, 5, 48, 3.0, 0.002)
TUNE_PARAM(seDeBase, 50, 0, 60, 8.0, 0.002)
TUNE_PARAM(seDePvCoeff, 400, 0, 450, 30.0, 0.002)

TUNE_PARAM(quietLmrBase, 1690, 0, 2535, 128.0, 0.002)
TUNE_PARAM(quietLmrMult, 366, 244, 549, 32.0, 0.002)

TUNE_PARAM(tacticalLmrBase, 338, 244, 549, 32.0, 0.002)
TUNE_PARAM(tacticalLmrMult, 320, 244, 549, 32.0, 0.002)

TUNE_PARAM(histBonusQuadratic, 16, 0, 32, 1.0, 0.002)
TUNE_PARAM(histBonusLinear, 32, 0, 64, 2.0, 0.002)
TUNE_PARAM(histBonusConst, 16, 0, 32, 1.0, 0.002)
TUNE_PARAM(histBonusMax, 1200, 400, 4800, 100.0, 0.002)

TUNE_PARAM(quietHistFactoriserScale, 16, 0, 64, 2.0, 0.002)
TUNE_PARAM(quietHistFactoriserMax, 2048, 2048, 20000, 150.0, 0.002)
TUNE_PARAM(quietHistBucketMax, 6144, 2048, 20000, 300.0, 0.002)

TUNE_PARAM(tacticalHistMax, 8192, 4096, 20000, 300.0, 0.002)

TUNE_PARAM(continuationHistMax, 16384, 8192, 32767, 300.0, 0.002)

TUNE_PARAM(corrHistMaxAdjust, 22528, 16384, 32767, 800.0, 0.002)
TUNE_PARAM(corrHistWeightQuadratic, 4, 0, 8, 0.5, 0.002)
TUNE_PARAM(corrHistWeightLinear, 8, 0, 16, 0.5, 0.002)
TUNE_PARAM(corrHistWeightConst, 4, 0, 8, 0.5, 0.002)
TUNE_PARAM(corrHistWeightMax, 128, 1, 345, 17.0, 0.002)

TUNE_PARAM(eval50mrScale, 200, 150, 250, 5.0, 0.002)
TUNE_PARAM(evalMatBase, 3072, 2464, 4096, 82.0, 0.002)
TUNE_PARAM(evalMatMult, 16, 14, 18, 0.5, 0.002)