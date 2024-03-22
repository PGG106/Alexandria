#pragma once

struct Position;

// Returns if the position is a draw because there isn't enough mating material
[[nodiscard]] bool MaterialDraw(const Position* pos);
// position evaluation
[[nodiscard]] int EvalPosition(Position* pos);
