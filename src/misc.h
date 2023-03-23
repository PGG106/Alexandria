#pragma once
#include "board.h"
#include "search.h"
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "windows.h"
#endif

uint64_t GetTimeMs();

std::vector<std::string> split_command(const std::string& command);

bool Contains(std::vector<std::string> tokens, std::string key);

void dbg_mean_of(int val);

void dbg_print();
