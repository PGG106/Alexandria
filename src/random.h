#pragma once

#include "types.h"

// pseudo random number state
extern uint64_t random_state;

// generate 64-bit pseudo legal numbers
[[nodiscard]] uint64_t GetRandomU64Number();
