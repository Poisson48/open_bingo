#pragma once

#include "bingotypes.h"

#include <functional>
#include <random>

namespace core {

using Rng = std::function<double()>;

Requirements calcRequirements(const Project& project);

GenerateResult generateAll(Project& project, const Rng& rng = Rng{});

void reshuffleGrid(Project& project, int playerIdx, const Rng& rng);

PlayerGrid generatePlayerGrid(const Project& project,
                              const std::string& playerName,
                              int rows, int cols, bool hasCenter, int available,
                              const Rng& rng);

// Play helpers
int computeScore(const PlayerGrid& grid, const std::vector<std::vector<bool>>& checks);

// Bingo lignes / colonnes ; diagonales seulement si rows == cols (grille carrée).
std::vector<std::vector<std::pair<int, int>>> detectBingo(
    const std::vector<std::vector<bool>>& checks, int rows, int cols);

// Compat : grille carrée N×N.
inline std::vector<std::vector<std::pair<int, int>>> detectBingo(
    const std::vector<std::vector<bool>>& checks, int N)
{
    return detectBingo(checks, N, N);
}

std::string detectLineType(const std::vector<std::pair<int, int>>& line, int rows, int cols);

inline std::string detectLineType(const std::vector<std::pair<int, int>>& line, int N)
{
    return detectLineType(line, N, N);
}

} // namespace core
