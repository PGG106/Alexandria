#pragma once

#include "types.h"

struct Position;
struct SearchData;
struct SearchStack;
struct MoveList;

// Clean all the history tables
void CleanHistories(SearchData* sd);
