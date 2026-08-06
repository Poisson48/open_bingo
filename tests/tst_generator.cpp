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
};

QTEST_MAIN(GeneratorTest)
#include "tst_generator.moc"
