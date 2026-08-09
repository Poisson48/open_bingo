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
    int         hp     = 5;
    int         number = 1;   // n° assigné (plusieurs gages peuvent partager le même)
    int         rate   = 100; // poids relatif (%) au tirage parmi ceux du même n°
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
    // Dimensions de grille (source de vérité). gridSize reste pour compat JSON /
    // anciens lecteurs (= max(rows, cols) après normalisation).
    int                      gridRows   = 5;
    int                      gridCols   = 5;
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
    int  rows      = 0;
    int  cols      = 0;
    int  N         = 0; // alias historique = max(rows, cols) ; préférer rows/cols
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

// Clamp 2–12, migre gridSize → rows/cols, resynchronise gridSize = max(rows, cols).
inline void normalizeGridDims(Project& p)
{
    if (p.gridRows <= 0 && p.gridCols <= 0) {
        const int n = p.gridSize > 0 ? p.gridSize : 5;
        p.gridRows = n;
        p.gridCols = n;
    } else {
        if (p.gridRows <= 0)
            p.gridRows = p.gridSize > 0 ? p.gridSize : 5;
        if (p.gridCols <= 0)
            p.gridCols = p.gridSize > 0 ? p.gridSize : 5;
    }
    if (p.gridRows < 2) p.gridRows = 2;
    if (p.gridRows > 12) p.gridRows = 12;
    if (p.gridCols < 2) p.gridCols = 2;
    if (p.gridCols > 12) p.gridCols = 12;
    p.gridSize = p.gridRows > p.gridCols ? p.gridRows : p.gridCols;
}

inline bool projectHasFreeCenter(const Project& p)
{
    return p.freeCenter && (p.gridRows % 2 == 1) && (p.gridCols % 2 == 1);
}

} // namespace core
