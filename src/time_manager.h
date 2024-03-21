#pragma once

struct SearchInfo;
struct S_ThreadData;

void Optimum(SearchInfo* info, int time, int inc);
[[nodiscard]] bool StopEarly(const SearchInfo* info);
[[nodiscard]] bool TimeOver(const SearchInfo* info);
[[nodiscard]] bool NodesOver(const SearchInfo* info);
void ScaleTm(S_ThreadData* td, const int bestMoveStabilityFactor);
