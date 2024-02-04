#include "tune.h"

std::vector<tunable_param> tunableParams;
std::unordered_map<std::string, int> tunedValues;

void addTune(std::string name, std::string type, int curr_value, int min_value, int max_value, float C_end, float R_end) {
	tunableParams.emplace_back(name, type, curr_value, min_value, max_value, C_end, R_end);
	tunedValues.emplace(name, curr_value);
}

void InitTunable() {
    // Examples
    /*
    addTune("v1", "int", 50, 35, 65, 5, 0.0020);
    addTune("v2", "int", 75, 55, 95, 5, 0.0020);
    addTune("v3", "int", 75, 55, 85, 5, 0.0020);
    addTune("v4", "int", 70, 50, 90, 5, 0.0020);
    addTune("v5", "int", 300, 200, 400, 40, 0.0020);
    addTune("v6", "int", 250, 200, 300, 20, 0.0020);
    addTune("v7", "int", 120, 70, 170, 20, 0.0020);
    addTune("v8", "int", 95, 45, 145, 20, 0.0020);
    addTune("v9", "int", 85, 35, 135, 20, 0.0020);
    addTune("v10", "int", 80, 30, 130, 20, 0.0020);
    addTune("v11", "int", 162, 112, 212, 20, 0.0020);
    addTune("v12", "int", 148, 98, 198, 20, 0.0020);
    */
}

void updateTuneVariable(std::string tune_variable_name, int value)
{
	tunedValues[tune_variable_name] = value;
}

