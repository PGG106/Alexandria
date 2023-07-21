#include "init.h"
#include "uci.h"
#include "move.h"
#include <iostream>

int main([[maybe_unused]] int argc, char** argv) {
    // init all
    InitAll();
    // connect to the GUI
    UciLoop(argv);
    return 0;
}
