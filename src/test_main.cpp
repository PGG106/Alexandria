#include "../src/Board.h"
#include <iostream>
void test_get_fen();

void run_tests()
{
	test_get_fen();
}

//Parses a fen and checks that the fen returned by get_fen matches	
void test_get_fen()
{
	std::cout << "Running test_get_fen\n";
	//Create a board object
	S_Board pos;
	//Set up a position
	auto fen_to_parse = "rnb1kb1r/pppqpp1p/5n2/1B1p4/3PPPp1/3Q3N/PPP3PP/RNB1K2R b KQkq f3 0 6";
	parse_fen(fen_to_parse, &pos);
	std::string fen = get_fen(&pos);
	assert(fen_to_parse == fen);
	std::cout << "test_get_fen cleared\n";
	return;
}