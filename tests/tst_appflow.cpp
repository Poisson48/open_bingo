#include "../src/app/appcontroller.h"

#include <QGuiApplication>
#include <QTemporaryDir>
#include <QtTest>

// Parcours utilisateur réel via AppController (DB SQLite temporaire).
class AppFlowTest : public QObject
{
    Q_OBJECT
private slots:
    void fullBingoWorkflow()
    {
        QTemporaryDir tmp;
        QVERIFY2(tmp.isValid(), "temp dir");
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY2(controller.init(), "AppController::init");

        // init() crée une démo : on part d'une base vide pour ce parcours.
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        const QString id = controller.createProject();
        QVERIFY(!id.isEmpty());
        QCOMPARE(controller.currentProjectId(), id);

        controller.setTitle(QStringLiteral("Test soirée"));
        controller.setGridSize(3);
        // Remplacer les joueurs démo par 2 joueurs de test
        while (controller.players().size() > 2)
            controller.removePlayer(controller.players().size() - 1);
        while (controller.players().size() < 2)
            controller.addPlayer();
        controller.setPlayerName(0, QStringLiteral("J1"));
        controller.setPlayerName(1, QStringLiteral("J2"));
        // Remplacer les phrases
        while (controller.cases().size() > 0)
            controller.removeCase(0);
        controller.addCase(QStringLiteral("Phrase A"), 2, 100);
        controller.addCase(QStringLiteral("Phrase B"), 3, 100);
        controller.addCase(QStringLiteral("Phrase C"), 1, 100);
        controller.addCase(QStringLiteral("Phrase D"), 4, 100);
        controller.addCase(QStringLiteral("Phrase E"), 5, 100);
        controller.addCase(QStringLiteral("Phrase F"), 1, 100);
        controller.addCase(QStringLiteral("Phrase G"), 2, 100);
        controller.addCase(QStringLiteral("Phrase H"), 3, 100);
        controller.addCase(QStringLiteral("Phrase I"), 1, 100);

        const QString genMsg = controller.generateAll();
        QVERIFY2(!genMsg.contains(QStringLiteral("Aucun")), qPrintable(genMsg));
        QCOMPARE(controller.grids().size(), 2);

        const auto grids = controller.grids();
        const auto grid0 = grids[0].toMap();
        const QString player = grid0.value(QStringLiteral("player")).toString();
        QVERIFY(!player.isEmpty());
        const QVariantList cellRows = grid0.value(QStringLiteral("cells")).toList();
        QVERIFY(cellRows.size() >= controller.gridSize());

        QVariantList checks;
        const int n = controller.gridSize();
        for (int r = 0; r < n; ++r) {
            QVariantList row;
            for (int c = 0; c < n; ++c)
                row.append(false);
            checks.append(row);
        }
        controller.savePlayChecks(player, checks);
        QVERIFY(!controller.loadPlayChecks(player).isEmpty());

        const QString exportPath = tmp.path() + QStringLiteral("/export.json");
        QVERIFY(controller.exportCurrentJson(exportPath));

        controller.deleteProject(id);
        QCOMPARE(controller.projects()->rowCount(), 0);

        const int imported = controller.importAllJsonFile(exportPath);
        QVERIFY(imported >= 1);
        QVERIFY(controller.projects()->rowCount() >= 1);
    }

    void demoProjectIsPlayable()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        // init() auto-seeds demo when empty
        QVERIFY2(controller.projects()->rowCount() >= 1, "demo auto-seeded");
        QVERIFY(controller.grids().size() >= 2);
        QCOMPARE(controller.grids().size(), controller.players().size());
        const auto grid0 = controller.grids()[0].toMap();
        const auto cells = grid0.value(QStringLiteral("cells")).toList();
        QCOMPARE(cells.size(), controller.gridSize());
        QVERIFY(cells[0].toList().size() >= controller.gridSize());
    }

    void generateAllRequiresCases()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", (tmp.path() + QStringLiteral("/empty")).toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        // Clear auto-demo then create bare project without cases
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        // seedDemo creates playable; we need empty cases — use generate after wiping via save
        const QString id = controller.seedDemoProject();
        QVERIFY(!id.isEmpty());
        QVERIFY(controller.openProject(id));
        // Remove all cases then try generate
        while (controller.cases().size() > 0)
            controller.removeCase(0);
        const QString msg = controller.generateAll();
        QVERIFY2(msg.contains(QStringLiteral("case")), qPrintable(msg));
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    AppFlowTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_appflow.moc"
