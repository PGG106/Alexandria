#include "tune.h"

std::vector<tunable_param> tunable_params;
std::unordered_map<std::string, int> tuned_values;

void addTune(std::string name, std::string type, int curr_value, int min_value, int max_value, float C_end) {
	tunable_params.emplace_back(name, type, curr_value, min_value, max_value, C_end);
	tuned_values.emplace(name, curr_value);
}

void InitTunable() {
	//addTune("lmrdivisor", "int", 12, 11, 17, 22.1);
}

void updateTuneVariable(std::string tune_variable_name, int value)
{
	tuned_values[tune_variable_name] = value;
}

