#pragma once

#include "types.h"

struct Position;

[[nodiscard]] Bitboard GeneratePosKey(const Position* pos);
