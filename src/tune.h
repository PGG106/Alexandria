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

// Very cursed macro wizardry to set and fetch the values without having to manually swap in hashtable accesses, partially stolen from SP
// Start with the case where we are actually tuning
#ifdef TUNE
#define TUNE_PARAM(Name, Default) \
        inline auto Name() { return tunedValues[#Name]; }
#else
#define TUNE_PARAM(Name, Default) \
		constexpr auto Name() -> int { return Default; }
#endif

// This class acts as a fancy string constructor, it's used just to store all the info OB wants for a tune
class tunable_param {
public:
	std::string name;
	std::string type;
	float currValue;
	float minValue;
	float maxValue;
	float C_end;
	float R_end;
	tunable_param(std::string param_name, std::string param_type, float param_curr_value, float param_min_value, float param_max_value, float param_C_end, float param_R_end) {
		this->name = param_name;
		this->type = param_type;
		this->currValue = param_curr_value;
		this->minValue = param_min_value;
		this->maxValue = param_max_value;
		this->C_end = param_C_end;
		this->R_end = param_R_end;
	}

	friend std::ostream& operator<<(std::ostream& os, const tunable_param& param)
	{
		os << param.name << ", ";
		os << param.type << ", ";
		os << param.currValue << ", ";
		os << param.minValue << ", ";
		os << param.maxValue << ", ";
		os << param.C_end << ", ";
		os << param.R_end;
		return os;
	}
};

// Data structures to handle the output of the params and the value setting at runtime
extern std::vector<tunable_param> tunableParams;
extern std::unordered_map<std::string, int> tunedValues;
// Actual functions to init and update variables
void addTune(std::string name, std::string type, int curr_value, int min_value, int max_value, float C_end, float R_end);
// Handles the update of a variable being tuned
void updateTuneVariable(std::string tune_variable_name, int value);


inline void InitTunable() {
	// Example
	addTune("iirDepth", "int", 4, 1, 7, 122, 254);
}

// Giant wasteland of tunable params
TUNE_PARAM(iirDepth, 4);
