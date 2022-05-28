#pragma once
#include "Board.h"

#ifdef _MSC_VER
#  define U64(u) (u##ui64)
#else
#  define U64(u) (u##ULL)
#endif

extern const Bitboard Random64Poly[781];

Bitboard PolyKeyFromBoard(S_Board* pos);

int GetBookMove(S_Board* pos);

void InitPolyBook();