#include "tune.h"

std::vector<tunable_param> tunableParams;
std::unordered_map<std::string, int> tunedValues;

void addTune(std::string name, std::string type, int curr_value, int min_value, int max_value, float C_end, float R_end) {
    tunableParams.emplace_back(name, type, curr_value, min_value, max_value, C_end, R_end);
    tunedValues.emplace(name, curr_value);
}

void updateTuneVariable(std::string tune_variable_name, int value)
{
    tunedValues[tune_variable_name] = value;
}
