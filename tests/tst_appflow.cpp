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

        const QString id = controller.createProject();
        QVERIFY(!id.isEmpty());
        QCOMPARE(controller.currentProjectId(), id);

        controller.setTitle(QStringLiteral("Test soirée"));
        controller.setGridSize(3);
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
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    AppFlowTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_appflow.moc"
