#include "Board.h"
#include "init.h"
#include "search.h"
#include "ttable.h"
#include "uci.h"


int main([[maybe_unused]] int argc, char** argv) {
	setvbuf(stdout, NULL, _IONBF, 0);
	// init all
	init_all();

	// connect to the GUI
	Uci_Loop(argv);
	
	return 0;
}
