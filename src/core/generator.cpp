#include "generator.h"

#include <algorithm>
#include <random>

namespace core {
namespace {

template<typename T>
std::vector<T> shuffleCopy(std::vector<T> arr, std::mt19937& eng)
{
    std::shuffle(arr.begin(), arr.end(), eng);
    return arr;
}

std::mt19937& threadLocalRng()
{
    thread_local std::mt19937 eng{ std::random_device{}() };
    return eng;
}

double roll(const Rng& rng)
{
    if (rng)
        return rng();
    return std::uniform_real_distribution<>(0.0, 100.0)(threadLocalRng());
}

PlayerGrid buildGrid(const std::string& playerName,
                     const std::vector<GridCell>& cellData,
                     int N, bool hasCenter)
{
    const int centerRow = N / 2;
    const int centerCol = N / 2;
    int idx = 0;
    PlayerGrid grid;
    grid.player = playerName;
    grid.cells.resize(N);

    for (int r = 0; r < N; ++r) {
        grid.cells[r].resize(N);
        for (int c = 0; c < N; ++c) {
            if (hasCenter && r == centerRow && c == centerCol) {
                grid.cells[r][c] = { "FREE", 0, 100, "", 0, true };
            } else {
                grid.cells[r][c] = cellData.at(static_cast<size_t>(idx++));
            }
        }
    }
    return grid;
}

} // namespace

Requirements calcRequirements(const Project& project)
{
    Requirements req;
    req.N = project.gridSize;
    req.total = req.N * req.N;
    req.hasCenter = project.freeCenter && (req.N % 2 == 1);
    req.available = req.hasCenter ? req.total - 1 : req.total;
    req.players = static_cast<int>(project.players.size());
    req.minCases = req.available;
    return req;
}

PlayerGrid generatePlayerGrid(const Project& project,
                              const std::string& playerName,
                              int N, bool hasCenter, int available,
                              const Rng& rng)
{
    std::vector<GridCell> included;
    std::vector<GridCell> excluded;

    for (const auto& c : project.cases) {
        const int rate = c.rate;
        if (rate == 0)
            continue;
        GridCell cell{ c.label, c.points, c.rate, "", 0, false };
        if (roll(rng) < rate)
            included.push_back(cell);
        else
            excluded.push_back(cell);
    }

    auto& eng = threadLocalRng();
    auto shuffledIncluded = shuffleCopy(included, eng);
    auto shuffledExcluded = shuffleCopy(excluded, eng);

    std::vector<GridCell> cells;
    if (static_cast<int>(shuffledIncluded.size()) >= available) {
        cells.assign(shuffledIncluded.begin(), shuffledIncluded.begin() + available);
    } else {
        cells = shuffledIncluded;
        const int need = available - static_cast<int>(cells.size());
        const int take = std::min(need, static_cast<int>(shuffledExcluded.size()));
        cells.insert(cells.end(), shuffledExcluded.begin(), shuffledExcluded.begin() + take);
    }

    if (static_cast<int>(cells.size()) < available) {
        auto pool = shuffleCopy(included, eng);
        if (pool.empty())
            pool = shuffleCopy(excluded, eng);
        if (pool.empty()) {
            for (const auto& c : project.cases) {
                if (c.rate != 0)
                    pool.push_back({ c.label, c.points, c.rate, "", 0, false });
            }
            pool = shuffleCopy(pool, eng);
        }
        size_t i = 0;
        while (static_cast<int>(cells.size()) < available) {
            cells.push_back(pool[i % pool.size()]);
            ++i;
        }
    }

    cells = shuffleCopy(cells, eng);
    return buildGrid(playerName, cells, N, hasCenter);
}

GenerateResult generateAll(Project& project, const Rng& rng)
{
    GenerateResult result;
    const auto req = calcRequirements(project);

    if (req.players == 0) {
        result.error = true;
        result.message = "Ajoutez au moins un joueur dans la configuration.";
        return result;
    }
    if (project.cases.empty()) {
        result.error = true;
        result.message = "Ajoutez au moins une case dans l'onglet Cases.";
        return result;
    }

    project.grids.clear();
    project.grids.reserve(project.players.size());
    for (const auto& p : project.players) {
        project.grids.push_back(
            generatePlayerGrid(project, p.name, req.N, req.hasCenter, req.available, rng));
    }

    result.repeats = static_cast<int>(project.cases.size()) < req.available;
    return result;
}

void reshuffleGrid(Project& project, int playerIdx, const Rng& rng)
{
    if (playerIdx < 0 || playerIdx >= static_cast<int>(project.grids.size()))
        return;
    const auto req = calcRequirements(project);
    const auto& grid = project.grids[static_cast<size_t>(playerIdx)];
    project.grids[static_cast<size_t>(playerIdx)] =
        generatePlayerGrid(project, grid.player, req.N, req.hasCenter, req.available, rng);
}

int computeScore(const PlayerGrid& grid, const std::vector<std::vector<bool>>& checks)
{
    int score = 0;
    const int N = static_cast<int>(grid.cells.size());
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (r < static_cast<int>(checks.size()) &&
                c < static_cast<int>(checks[r].size()) &&
                checks[r][c]) {
                score += grid.cells[r][c].points;
            }
        }
    }
    return score;
}

std::vector<std::vector<std::pair<int, int>>> detectBingo(
    const std::vector<std::vector<bool>>& checks, int N)
{
    std::vector<std::vector<std::pair<int, int>>> lines;

    auto rowComplete = [&](int r) {
        for (int c = 0; c < N; ++c)
            if (!checks[r][c]) return false;
        return true;
    };
    auto colComplete = [&](int c) {
        for (int r = 0; r < N; ++r)
            if (!checks[r][c]) return false;
        return true;
    };

    for (int r = 0; r < N; ++r) {
        if (rowComplete(r)) {
            std::vector<std::pair<int, int>> line;
            for (int c = 0; c < N; ++c)
                line.emplace_back(r, c);
            lines.push_back(std::move(line));
        }
    }
    for (int c = 0; c < N; ++c) {
        if (colComplete(c)) {
            std::vector<std::pair<int, int>> line;
            for (int r = 0; r < N; ++r)
                line.emplace_back(r, c);
            lines.push_back(std::move(line));
        }
    }

    bool mainDiag = true;
    for (int i = 0; i < N; ++i)
        if (!checks[i][i]) mainDiag = false;
    if (mainDiag) {
        std::vector<std::pair<int, int>> line;
        for (int i = 0; i < N; ++i)
            line.emplace_back(i, i);
        lines.push_back(std::move(line));
    }

    bool antiDiag = true;
    for (int i = 0; i < N; ++i)
        if (!checks[i][N - 1 - i]) antiDiag = false;
    if (antiDiag) {
        std::vector<std::pair<int, int>> line;
        for (int i = 0; i < N; ++i)
            line.emplace_back(i, N - 1 - i);
        lines.push_back(std::move(line));
    }

    return lines;
}

std::string detectLineType(const std::vector<std::pair<int, int>>& line, int N)
{
    if (line.empty())
        return {};
    const bool sameRow = std::all_of(line.begin(), line.end(),
                                     [&](const auto& p) { return p.first == line[0].first; });
    if (sameRow)
        return "line";
    const bool sameCol = std::all_of(line.begin(), line.end(),
                                     [&](const auto& p) { return p.second == line[0].second; });
    if (sameCol)
        return "column";
    const bool mainD = std::all_of(line.begin(), line.end(),
                                   [](const auto& p) { return p.first == p.second; });
    if (mainD)
        return "diagonal";
    const bool antiD = std::all_of(line.begin(), line.end(),
                                   [N](const auto& p) { return p.first + p.second == N - 1; });
    if (antiD)
        return "diagonal";
    return {};
}

} // namespace core
