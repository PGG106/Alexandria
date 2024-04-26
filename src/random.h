#pragma once

#include "types.h"

// pseudo random number state
extern uint64_t random_state;

// generate 32-bit pseudo legal numbers
[[nodiscard]] uint64_t GetRandomU32Number();

// generate 64-bit pseudo legal numbers
[[nodiscard]] Bitboard GetRandomBitboardNumber();
