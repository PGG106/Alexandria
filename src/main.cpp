#include "init.h"
#include "uci.h"
#include "tune.h"

int main([[maybe_unused]] int argc, char** argv) {
    // init all
    InitAll();
#ifdef TUNE
    InitTunable();
#endif
    // connect to the GUI
    UciLoop(argv);
    return 0;
}
