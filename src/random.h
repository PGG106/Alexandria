#pragma once

#include "board.h"

// pseudo random number state
extern unsigned int random_state;

// generate 32-bit pseudo legal numbers
unsigned int GetRandomU32Number();

// generate 64-bit pseudo legal numbers
Bitboard GetRandomBitboardNumber();

// generate magic number candidate
Bitboard GenerateMagicNumber();
