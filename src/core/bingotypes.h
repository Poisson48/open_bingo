#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace core {

struct Player {
    std::string name;
};

struct Case {
    std::string label;
    int         points = 1;
    int         rate   = 50;
};

struct Gage {
    std::string description;
    int         hp = 5;
};

struct ComboGages {
    std::string line;
    std::string column;
    std::string diagonal;
};

struct Multipliers {
    int line     = 2;
    int column   = 2;
    int diagonal = 3;
    int full     = 10;
};

struct GridCell {
    std::string label;
    int         points  = 0;
    int         rate    = 100;
    std::string gage;
    int         gageHP  = 0;
    bool        isFree  = false;
};

struct PlayerGrid {
    std::string                    player;
    std::vector<std::vector<GridCell>> cells;
};

struct Project {
    std::string              id;
    std::string              title;
    std::string              description;
    int64_t                  createdAt  = 0;
    int64_t                  updatedAt  = 0;
    int                      gridSize   = 5;
    std::vector<Player>      players;
    int                      startHP    = 20;
    bool                     freeCenter = true;
    bool                     gageMode   = false;
    ComboGages               comboGages;
    Multipliers              multipliers;
    std::vector<Case>        cases;
    std::vector<Gage>        gages;
    std::vector<PlayerGrid>  grids;
};

struct Requirements {
    int  N         = 0;
    int  total     = 0;
    bool hasCenter = false;
    int  available = 0;
    int  players   = 0;
    int  minCases  = 0;
};

struct GenerateResult {
    bool        error   = false;
    std::string message;
    bool        repeats = false;
};

} // namespace core
