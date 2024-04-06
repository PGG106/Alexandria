#include "init.h"
#include "uci.h"

int main(int argc, char** argv) {
    // Tables for move generation and precompute reduction values
    InitAll();
    // connect to the GUI
    UciLoop(argc, argv);
    return 0;
}
