
#include "misc.h"
#include "stdlib.h"
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

//Prints the uci output
void PrintUciOutput(const int score, const int depth, const S_ThreadData* td, const  S_UciOptions* options) {


	//This handles the basic console output
	long  time = GetTimeMs() - td->info.starttime;
	uint64_t nodes = td->info.nodes + getTotalNodes(options->Threads);

	uint64_t nps = nodes / (time + !time) * 1000;
	if (print_uci)
	{
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
		for (int count = 0; count < td->pv_table.pvLength[0]; count++) {
			// print PV move
			print_move(td->pv_table.pvArray[0][count]);
			printf(" ");
		}

		// print new line
		printf("\n");
	}
	else {

		//Convert time in seconds if possible
		std::string time_unit = "ms";
		float parsed_time = time;
		if (time >= 1000) {
			parsed_time = parsed_time / 1000;
			time_unit = 's';
		}
		//Convert score to a decimal format or to a mate string
		float parsed_score = 0;
		bool is_mate = false;
		if (score > -mate_value && score < -mate_score) {
			parsed_score = -(score + mate_value) / 2;
			is_mate = true;
		}
		else if (score > mate_score && score < mate_value) {
			parsed_score = (mate_value - score) / 2 + 1;
			is_mate = true;
		}
		else  parsed_score = static_cast<float>(score) / 100;


		std::cout << std::setw(3) << depth << "/";
		std::cout << std::left << std::setw(3) << td->info.seldepth;
		std::cout << std::right << std::setw(4) << std::setprecision(3) << parsed_time;
		std::cout << std::left << std::setw(2) << time_unit;
		std::cout << std::right << std::setw(5) << nodes / 1000 << "kn ";
		if (is_mate) std::cout << std::setw(1) << "M" << std::left << std::setw(3) << parsed_score;
		else std::cout << std::setw(5) << parsed_score;
		std::cout << std::right << std::setw(5) << nps / 1000 << "kn/s" << "  ";

		// loop over the moves within a PV line
		for (int count = 0; count < td->pv_table.pvLength[0]; count++) {
			// print PV move
			print_move(td->pv_table.pvArray[0][count]);
			printf(" ");
		}

		// print new line
		printf("\n");
	}

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