
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
		if (parsed_time >= 1000) {
			parsed_time = parsed_time / 1000;
			time_unit = 's';
			if (parsed_time >= 60)
			{
				parsed_time = parsed_time / 60;
				time_unit = 'm';
			}
		}

		// convert time to string
		std::stringstream time_stream;
		time_stream << std::setprecision(3) << parsed_time;
		std::string time_string = time_stream.str() + time_unit;

		//Convert score to a decimal format or to a mate string
		float parsed_score = 0;
		std::string score_unit = "";
		if (score > -mate_value && score < -mate_score) {
			parsed_score = std::abs((score + mate_value) / 2);
			score_unit = "-M";
		}
		else if (score > mate_score && score < mate_value) {
			parsed_score = (mate_value - score) / 2 + 1;
			score_unit = "+M";
		}
		else {
			parsed_score = static_cast<float>(score) / 100;
			if (parsed_score > 0)   score_unit = '+';
		}
		// convert score to string
		std::stringstream score_stream;
		score_stream << std::fixed << std::setprecision(2) << parsed_score;
		std::string score_string = score_unit + score_stream.str();
		int node_precision = 0;
		//convert nodes into string
		std::string node_unit = "n";
		float parsed_nodes = static_cast<float>(nodes);
		if (parsed_nodes >= 1000) {
			parsed_nodes = parsed_nodes / 1000;
			node_unit = "Kn";
			node_precision = 2;
			if (parsed_nodes >= 1000)
			{
				parsed_nodes = parsed_nodes / 1000;
				node_unit = "Mn";
			}
		}

		std::stringstream node_stream;
		node_stream << std::fixed << std::setprecision(node_precision) << parsed_nodes;
		std::string node_string = node_stream.str() + node_unit;

		//Pretty print search info
		std::cout << std::setw(3) << depth << "/";
		std::cout << std::left << std::setw(3) << td->info.seldepth;

		std::cout << std::right << std::setw(8) << time_string;
		std::cout << std::right << std::setw(10) << node_string;
		std::cout << std::setw(8) << std::right << score_string;
		std::cout << std::setw(8) << std::right << std::fixed << std::setprecision(0) << nps / 1000.0 << "kn/s" << " ";

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