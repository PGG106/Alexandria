#pragma once
#include "Board.h"
#include "stdio.h"
#include "move.h"
#include "PieceData.h"
#include "movegen.h"


#define take_back()                                                       \
    memcpy(pos->bitboards, pos->bitboards_copy, 96);                                \
    memcpy(occupancies, occupancies_copy, 24);                            \
    side = side_copy, enpassant = enpassant_copy, castle = castle_copy;   \




// get time in milliseconds
int get_time_ms();


// perft driver
static inline void perft_driver(int depth, S_Board* pos);
// perft test
unsigned long  long perft_test(int depth, S_Board* pos);