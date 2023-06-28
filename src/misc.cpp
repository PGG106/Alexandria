
#include "misc.h"
#include "io.h"
#include "threads.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <chrono>


uint64_t GetTimeMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

long long int _count, _accumulator;

void dbg_mean_of(int val) { _count++; _accumulator += val; }

void dbg_print() { std::cout << double(_accumulator) / _count << std::endl; }

//splits a string into a vector of tokens and returns it
std::vector<std::string> split_command(const std::string& command)
{

	std::stringstream stream(command);
	std::string intermediate;
	std::vector<std::string> tokens;

	while (std::getline(stream, intermediate, ' '))
	{
		tokens.push_back(intermediate);
	}

	return tokens;
}

//returns true if in a vector of string there's one that matches the key
bool Contains(std::vector<std::string> tokens, std::string key) {

	if (std::find(tokens.begin(), tokens.end(), key) != tokens.end()) {
		return true;
	}
	else {
		return false;
	}
	return false;
}
