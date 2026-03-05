#include "perft.h"
#include "position.h"
#include "piece_data.h"
#include "makemove.h"
#include "misc.h"
#include "movegen.h"
#include "move.h"
#include <iostream>

// leaf nodes (number of positions reached during the test of the move generator
// at a given depth)
uint64_t nodes;
