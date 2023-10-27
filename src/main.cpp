#include "init.h"
#include "uci.h"
#include "tune.h"

int main([[maybe_unused]] int argc, char** argv) {
#ifdef TUNE
    InitTunable();
#endif
    // Tables for move generation and precompute reduction values
    InitAll();
    // connect to the GUI
    UciLoop(argv);
    return 0;
}
