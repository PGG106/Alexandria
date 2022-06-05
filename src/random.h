#pragma once
#include "Board.h"

// pseudo random number state
extern unsigned int random_state;

// generate 32-bit pseudo legal numbers
unsigned int get_random_U32_number();

// generate 64-bit pseudo legal numbers
Bitboard get_random_Bitboard_number();

// generate magic number candidate
Bitboard generate_magic_number();