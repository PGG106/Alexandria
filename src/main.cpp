#include "board.h"
#include "init.h"
#include "uci.h"

int main([[maybe_unused]] int argc, char** argv) {

	// init all
	InitAll();

	// connect to the GUI
	UciLoop(argv);

	return 0;
}
