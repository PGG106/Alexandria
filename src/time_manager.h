#pragma once

struct S_SearchINFO;
struct S_ThreadData;

void Optimum(S_SearchINFO* info, int time, int inc);
[[nodiscard]] bool StopEarly(const S_SearchINFO* info);
[[nodiscard]] bool TimeOver(const S_SearchINFO* info);
[[nodiscard]] bool NodesOver(const S_SearchINFO* info);
void ScaleTm(S_ThreadData* td, const int bestmoveStabilityFactor);
