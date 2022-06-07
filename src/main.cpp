

// system headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include"Board.h"
#include "init.h"
#include "search.h"
#include "uci.h"
#include "hashkey.h"
#include "ttable.h"
#include "perft.h"
#include "PieceData.h"
#include "attack.h"

int main(int argc, char** argv)
{


	// init all
	init_all();
	S_Board pos[1];
	S_SearchINFO info[1];
	Reset_info(info);
	HashTable->pTable = NULL;
	InitHashTable(HashTable);
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	/*


 position startpos moves b1c3 d7d5 d2d4 g8f6 g1f3 b8c6 c1f4 e7e6 e2e3 f8d6 f1d3 e8g8 f4g3 a7a6 a1c1 b7b5 e1g1 c8b7 a2a3 d8e7 f1e1 f8d8 e3e4 d5e4 c3e4 d6g3 e4g3 a8c8 h2h3 b5b4 a3b4 c6b4 d3e4 b4d5 f3e5 f6e4 g3e4 d5f4 d1g4 f7f5 g4f4 f5e4 c1d1 d8f8 f4e3 f8f5 g2g4 f5f8 c2c4 f8d8 g4g5 d8e8 e3g3 c8d8 c4c5 b7d5 c5c6 d8b8 e1e2 b8b3 g3g4 e7b4 e5d7 b3d3 d1d3 e4d3 d7f6 g8f8 e2e3 b4c4 f6d5 c4d5 e3d3 d5c6 d3c3 c6d6 g4e4 f8g8 c3c6 d6d5 e4d5 e6d5 c6c7 e8e4 c7a7 e4d4 a7a6 d4d1 g1g2 d5d4 h3h4 g8f7 a6b6 d4d3 b6d6 f7e7 d6d4 h7h6 g5h6 g7h6 g2f3 d1d2 b2b3 e7e6 h4h5 e6e7 f3e3 d2e2 e3d3 e2f2 d4d5 f2b2 d3c3 b2b1 b3b4 e7f6 d5c5 b1c1 c3d4 c1d1 d4c4 d1h1 c4b5 h1b1 b5a5 b1a1 a5b6 a1b1 b4b5 b1e1 b6b7 e1e7 b7a6 f6e6 b5b6 e6d6 c5b5 e7e8 b6b7 d6c7 a6a7 c7d6 b7b8Q e8b8





	*/



	// debug mode variable
	int debug =0;

	// if debugging
	if (debug)
	{


		parse_fen(start_position, pos);
		perft_test(7, pos);



	}

	else
		// connect to the GUI
		Uci_Loop(pos, info, argv);

	return 0;
}

