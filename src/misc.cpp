
#include "misc.h"
#include "stdlib.h"
#include "io.h"
#include "threads.h"
#include <iostream>
#include <sstream>
#include <algorithm>


long GetTimeMs() {
#ifdef WIN32
	return GetTickCount();
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
#endif
}

//Prints the uci output
void PrintUciOutput(const int score, const int depth, const S_ThreadData* td, const  S_UciOptions* options) {

	//This handles the basic console output
	long  time = GetTimeMs() - td->info.starttime;
	uint64_t nodes = td->info.nodes + getTotalNodes(options->Threads);

	uint64_t nps = nodes / (time + !time) * 1000;

	if (score > -mate_value && score < -mate_score)
		std::cout << "info score mate " << -(score + mate_value) / 2 << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
		" nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

	else if (score > mate_score && score < mate_value)
		std::cout << "info score mate " << (mate_value - score) / 2 + 1 << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
		" nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

	else
		std::cout << " info score cp " << score << " depth " << depth << " seldepth " << td->info.seldepth << " multipv " << options->MultiPV << " nodes " << nodes <<
		" nps " << nps << " time " << GetTimeMs() - td->info.starttime << " pv ";

	// loop over the moves within a PV line
	for (int count = 0; count < td->ss.pvLength[0]; count++) {
		// print PV move
		print_move(td->ss.pvArray[0][count]);
		printf(" ");
	}

	// print new line
	printf("\n");

	return;
}

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
bool contains(std::vector<std::string> tokens, std::string key) {

	if (std::find(tokens.begin(), tokens.end(), key) != tokens.end()) {
		return true;
	}
	else {
		return false;
	}
	return false;
}