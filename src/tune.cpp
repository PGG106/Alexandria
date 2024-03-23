#include "tune.h"
std::unordered_map<std::string, tunable_param> tunableParams;

const int &addTune(std::string name, int defaultValue, int curr_value, int min_value, int max_value, float C_end, float R_end)
{
    tunable_param &param = tunableParams[name];
    param = tunable_param{name, defaultValue, curr_value, min_value, max_value, C_end, R_end};
    return param.currValue;
}

void updateTuneVariable(std::string tune_variable_name, int value)
{
    tunable_param &param = tunableParams[tune_variable_name];
    param.currValue = value;
}
