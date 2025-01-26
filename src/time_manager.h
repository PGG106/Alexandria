#pragma once

struct SearchInfo;
struct ThreadData;

void Optimum(int time, int inc);
[[nodiscard]] bool StopEarly();
[[nodiscard]] bool TimeOver();
[[nodiscard]] bool NodesOver();
void ScaleTm(const int bestMoveStabilityFactor, const int evalStabilityFactor);