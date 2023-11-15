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
To use said variable we simply have to swap out the previous magic number with the map entry (tuned_values[<parameter name>]).
The creation of the uci boilerplate strings and the handling of setoption is automated.
*/

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
	tunable_param(std::string param_name, std::string param_type, float param_curr_value, float param_min_value, float param_max_value, float param_C_end, float param_R_end = 0.002) {
		this->name = param_name;
		this->type = param_type;
		this->currValue = param_curr_value;
		this->minValue = param_min_value;
		this->maxValue = param_max_value;
		this->C_end = param_C_end;
		this->R_end = param_R_end;
	}
};

inline std::ostream& operator<<(std::ostream& os, const tunable_param& param)
{
	os << param.name <<", ";
	os << param.type << ", ";
	os << param.currValue << ", ";
	os << param.minValue << ", ";
	os << param.maxValue << ", ";
	os << param.C_end << ", ";
	os << param.R_end;
	return os;
}
// This contains all the tunable parameters informations, the information in here is never altered except for when a new entry gets added
extern std::vector<tunable_param> tunableParams;
// This is the map where the actual values of the variables being tuned are stored
extern std::unordered_map<std::string, int> tunedValues;
void InitTunable();
// Handles the update of a variable being tuned
void updateTuneVariable(std::string tune_variable_name, int value);
