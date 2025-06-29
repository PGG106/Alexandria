#pragma once

struct SearchInfo;
struct ThreadData;

void Optimum(SearchInfo* info, int time, int inc);
[[nodiscard]] bool StopEarly(const SearchInfo* info);
[[nodiscard]] bool TimeOver(const SearchInfo* info);
[[nodiscard]] bool NodesOver(const SearchInfo* info);
void ScaleTm(ThreadData* td, const int bestMoveStabilityFactor, const int evalStabilityFactor);
void ForcedTm(ThreadData* td);
