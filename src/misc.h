#pragma once

#include "Board.h"
#include "search.h"
#include "stdio.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "windows.h"
#else
#include "string.h"
#include "sys/select.h"
#include "sys/time.h"
#include "unistd.h"
#endif
#include <string>

long GetTimeMs();

void PrintUciOutput(const int score, const int depth, const  S_ThreadData* td, const S_UciOptions* options);

std::vector<std::string> split_command(const std::string& command);

bool contains(std::vector<std::string> tokens, std::string key);