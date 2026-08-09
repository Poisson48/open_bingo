#include <QtTest>
#include "../src/core/generator.h"
#include "../src/core/jsoncodec.h"
#include <random>

class GeneratorTest : public QObject
{
    Q_OBJECT
private slots:
    void rateZeroExcluded()
    {
        core::Project p = core::JsonCodec::defaultProject();
        p.gridSize = 3;
        p.gridRows = 3;
        p.gridCols = 3;
        p.freeCenter = false;
        p.players = { { "A" } };
        p.cases = { { "never", 1, 0 }, { "always", 2, 100 } };

        std::mt19937 rng(42);
        core::Rng roll = [&rng]() {
            return std::uniform_real_distribution<>(0.0, 100.0)(rng);
        };

        auto grid = core::generatePlayerGrid(p, "A", 3, 3, false, 9, roll);
        for (const auto& row : grid.cells)
            for (const auto& cell : row)
                QVERIFY(cell.label != "never");
    }

    void generateAllRequiresPlayers()
    {
        core::Project p = core::JsonCodec::defaultProject();
        p.players.clear();
        p.cases = { { "x", 1, 100 } };
        const auto r = core::generateAll(p);
        QVERIFY(r.error);
    }

    void bingoLineClearsWhenUnchecked()
    {
        const int N = 3;
        std::vector<std::vector<bool>> checks(N, std::vector<bool>(N, false));
        // Ligne 0 complète → bingo
        for (int c = 0; c < N; ++c)
            checks[0][c] = true;
        QCOMPARE(core::detectBingo(checks, N).size(), 1);

        // Décocher une case → plus de bingo (le vert doit disparaître)
        checks[0][1] = false;
        QCOMPARE(core::detectBingo(checks, N).size(), 0);

        // Colonne complète
        for (int r = 0; r < N; ++r)
            checks[r][2] = true;
        QCOMPARE(core::detectBingo(checks, N).size(), 1);
        checks[1][2] = false;
        QCOMPARE(core::detectBingo(checks, N).size(), 0);

        // Diagonale
        for (int i = 0; i < N; ++i)
            checks[i][i] = true;
        QCOMPARE(core::detectBingo(checks, N).size(), 1);
        checks[1][1] = false;
        QCOMPARE(core::detectBingo(checks, N).size(), 0);
    }

    void rectangularBingoNoDiagonal()
    {
        const int rows = 3, cols = 4;
        std::vector<std::vector<bool>> checks(rows, std::vector<bool>(cols, false));
        // Ligne complète
        for (int c = 0; c < cols; ++c)
            checks[1][c] = true;
        QCOMPARE(core::detectBingo(checks, rows, cols).size(), 1);

        // Remplir une « diagonale » style carré — ne doit PAS compter
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                checks[r][c] = (r == c);
        QCOMPARE(core::detectBingo(checks, rows, cols).size(), 0);

        // Colonne complète
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                checks[r][c] = (c == 2);
        QCOMPARE(core::detectBingo(checks, rows, cols).size(), 1);
    }

    void generateRectangularGrid()
    {
        core::Project p = core::JsonCodec::defaultProject();
        p.gridRows = 3;
        p.gridCols = 4;
        p.gridSize = 4;
        p.freeCenter = false;
        p.players = { { "A" } };
        p.cases.clear();
        for (int i = 0; i < 12; ++i)
            p.cases.push_back({ "c" + std::to_string(i), 1, 100 });
        const auto r = core::generateAll(p);
        QVERIFY(!r.error);
        QCOMPARE(static_cast<int>(p.grids.size()), 1);
        QCOMPARE(static_cast<int>(p.grids[0].cells.size()), 3);
        QCOMPARE(static_cast<int>(p.grids[0].cells[0].size()), 4);
    }
};

QTEST_MAIN(GeneratorTest)
#include "tst_generator.moc"
