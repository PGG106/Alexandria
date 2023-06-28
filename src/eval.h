#pragma once

struct S_Board;

// Returns if the position is a draw because there isn't enough mating material
[[nodiscard]] bool MaterialDraw(const S_Board* pos);
// position evaluation
[[nodiscard]] int EvalPosition(const S_Board* pos);
