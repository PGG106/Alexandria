#pragma once

#include "types.h"

struct S_Board;

[[nodiscard]] Bitboard GeneratePosKey(const S_Board* pos);
