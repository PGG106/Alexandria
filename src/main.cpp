

// system headers
#include "Board.h"
#include "PieceData.h"
#include "attack.h"
#include "hashkey.h"
#include "init.h"
#include "nnue.h"
#include "perft.h"
#include "search.h"
#include "ttable.h"
#include "uci.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main([[maybe_unused]] int argc, char** argv) {
	// init all
	init_all();

	InitHashTable(HashTable, 16);
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	// connect to the GUI
	Uci_Loop(argv);

	return 0;
}
