#pragma once

#include <string>
#include <vector>
#include <cstdint>

[[nodiscard]] uint64_t GetTimeMs();

[[nodiscard]] std::vector<std::string> split_command(const std::string& command);

[[nodiscard]] bool Contains(const std::vector<std::string>& tokens, const std::string& key);

void dbg_mean_of(int val);

void dbg_print();
