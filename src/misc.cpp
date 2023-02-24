
#include "misc.h"
#include "io.h"
#include "threads.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>


long GetTimeMs() {
#ifdef WIN32
	return GetTickCount();
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
#endif
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