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
        p.freeCenter = false;
        p.players = { { "A" } };
        p.cases = { { "never", 1, 0 }, { "always", 2, 100 } };

        std::mt19937 rng(42);
        core::Rng roll = [&rng]() {
            return std::uniform_real_distribution<>(0.0, 100.0)(rng);
        };

        auto grid = core::generatePlayerGrid(p, "A", 3, false, 9, roll);
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
};

QTEST_MAIN(GeneratorTest)
#include "tst_generator.moc"
