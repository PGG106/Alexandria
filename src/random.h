#pragma once

#include "types.h"

// pseudo random number state
extern unsigned int random_state;

// generate 32-bit pseudo legal numbers
[[nodiscard]] unsigned int GetRandomU32Number();

// generate 64-bit pseudo legal numbers
[[nodiscard]] Bitboard GetRandomBitboardNumber();

// generate magic number candidate
[[nodiscard]] Bitboard GenerateMagicNumber();
