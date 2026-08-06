#include "../src/app/appcontroller.h"

#include <QFileInfo>
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
        QVERIFY2(controller.projects()->rowCount() >= 1, "demo auto-seeded");
        QVERIFY(controller.title().contains(QStringLiteral("Cinéma")));
        QVERIFY(controller.grids().size() >= 2);
        QCOMPARE(controller.grids().size(), controller.players().size());
        const auto grid0 = controller.grids()[0].toMap();
        const auto cells = grid0.value(QStringLiteral("cells")).toList();
        QCOMPARE(cells.size(), controller.gridSize());
        QVERIFY(cells[0].toList().size() >= controller.gridSize());

        // Swap deux cases non-FREE (0,0) et (0,1) si possible
        const auto before = cells[0].toList()[0].toMap().value(QStringLiteral("label")).toString();
        controller.swapGridCells(0, 0, 0, 0, 1);
        const auto afterRows = controller.grids()[0].toMap().value(QStringLiteral("cells")).toList();
        const auto after01 = afterRows[0].toList()[1].toMap().value(QStringLiteral("label")).toString();
        QCOMPARE(after01, before);

        controller.moveGrid(0, 1);
        QCOMPARE(controller.grids().size(), controller.players().size());
    }

    void pdfExportProducesFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        QVERIFY(controller.grids().size() >= 1);

        const QString pdfPath = qEnvironmentVariableIsSet("BINGO_PDF_OUT")
            ? QString::fromLocal8Bit(qgetenv("BINGO_PDF_OUT"))
            : (tmp.path() + QStringLiteral("/grilles.pdf"));
        QVERIFY2(controller.exportPdf(pdfPath), "exportPdf");
        QVERIFY(QFileInfo::exists(pdfPath));
        QVERIFY(QFileInfo(pdfPath).size() > 500);
    }

    void editCaseAndGridCellLabel()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        QVERIFY(controller.cases().size() >= 1);
        QVERIFY(controller.grids().size() >= 1);

        controller.updateCase(0, QStringLiteral("Phrase éditée"), 7, 80);
        QCOMPARE(controller.cases()[0].toMap().value(QStringLiteral("label")).toString(),
                 QStringLiteral("Phrase éditée"));
        QCOMPARE(controller.cases()[0].toMap().value(QStringLiteral("points")).toInt(), 7);

        // Case (0,0) : si FREE (centre impair), éditer (0,1).
        const auto rows = controller.grids()[0].toMap().value(QStringLiteral("cells")).toList();
        QVERIFY(rows.size() >= 1);
        const auto row0 = rows[0].toList();
        QVERIFY(row0.size() >= 2);
        const bool free00 = row0[0].toMap().value(QStringLiteral("isFree")).toBool();
        const int col = free00 ? 1 : 0;
        controller.setGridCellLabel(0, 0, col, QStringLiteral("Texte grille"), 4);
        const auto after = controller.grids()[0].toMap()
                               .value(QStringLiteral("cells")).toList()[0].toList()[col].toMap();
        QCOMPARE(after.value(QStringLiteral("label")).toString(), QStringLiteral("Texte grille"));
        QCOMPARE(after.value(QStringLiteral("points")).toInt(), 4);
    }

    void uncheckClearsBingoHighlight()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        const int n = controller.gridSize();
        QVERIFY(n >= 3);

        QVariantList checks;
        for (int r = 0; r < n; ++r) {
            QVariantList row;
            for (int c = 0; c < n; ++c)
                row.append(r == 0); // ligne 0 complète
            // QVariantList::append(QVariantList) aplatit — encapsuler.
            checks.append(QVariant(row));
        }
        QCOMPARE(controller.detectBingoLines(checks).size(), 1);

        // Décocher le milieu de la ligne → plus de bingo
        auto row0 = checks[0].toList();
        row0[1] = false;
        checks[0] = QVariant(row0);
        QCOMPARE(controller.detectBingoLines(checks).size(), 0);
    }

    void assignGridSwapsPlayers()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        QVERIFY(controller.grids().size() >= 2);

        const QString a = controller.grids()[0].toMap().value(QStringLiteral("player")).toString();
        const QString b = controller.grids()[1].toMap().value(QStringLiteral("player")).toString();
        QVERIFY(!a.isEmpty());
        QVERIFY(!b.isEmpty());
        QVERIFY(a != b);

        const auto cellsA = controller.grids()[0].toMap().value(QStringLiteral("cells"));
        controller.assignGridToPlayer(0, b);
        QCOMPARE(controller.grids()[0].toMap().value(QStringLiteral("player")).toString(), b);
        QCOMPARE(controller.grids()[1].toMap().value(QStringLiteral("player")).toString(), a);
        // Le contenu de la feuille 0 ne bouge pas.
        QCOMPARE(controller.grids()[0].toMap().value(QStringLiteral("cells")), cellsA);
    }

    void togglePlayCellSyncsSameLabel()
    {
        // Style Colo : un cochage propage le même libellé à toutes les grilles.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        const QString id = controller.createProject();
        QVERIFY(controller.openProject(id));
        controller.setTitle(QStringLiteral("Film sync"));
        controller.setGridSize(3);
        controller.setFreeCenter(false);
        controller.setGageMode(true);
        while (controller.players().size() > 2)
            controller.removePlayer(controller.players().size() - 1);
        while (controller.players().size() < 2)
            controller.addPlayer();
        controller.setPlayerName(0, QStringLiteral("Alice"));
        controller.setPlayerName(1, QStringLiteral("Bob"));
        while (controller.gages().size() > 0)
            controller.removeGage(0);
        controller.addGage(QStringLiteral("Boire une gorgée"), 1);
        while (controller.cases().size() > 0)
            controller.removeCase(0);
        // 9 cases identiques pour forcer le même texte partout
        for (int i = 0; i < 9; ++i)
            controller.addCase(QStringLiteral("Explosion"), 1, 100);
        QVERIFY2(!controller.generateAll().contains(QStringLiteral("Aucun")), "generate");
        QCOMPARE(controller.grids().size(), 2);

        const QString alice = QStringLiteral("Alice");
        const QString bob = QStringLiteral("Bob");
        const auto result = controller.togglePlayCell(alice, 0, 0);
        QVERIFY(result.value(QStringLiteral("checked")).toBool());
        const auto overlays = result.value(QStringLiteral("overlays")).toList();
        QVERIFY(!overlays.isEmpty());
        // Un seul joueur (Alice) : pas d'overlay pour Bob malgré la sync.
        for (const auto& ovV : overlays) {
            const auto ov = ovV.toMap();
            if (ov.value(QStringLiteral("kind")).toString() == QLatin1String("gage"))
                QCOMPARE(ov.value(QStringLiteral("player")).toString(), alice);
        }

        auto countChecked = [](const QVariantList& checks) {
            int n = 0;
            for (const auto& rowV : checks) {
                for (const auto& cell : rowV.toList())
                    if (cell.toBool()) ++n;
            }
            return n;
        };
        // Toutes les cases « Explosion » cochées chez Alice et Bob
        QCOMPARE(countChecked(controller.loadPlayChecks(alice)), 9);
        QCOMPARE(countChecked(controller.loadPlayChecks(bob)), 9);

        // Décocher propage aussi
        const auto off = controller.togglePlayCell(alice, 1, 1);
        QVERIFY(!off.value(QStringLiteral("checked")).toBool());
        QCOMPARE(countChecked(controller.loadPlayChecks(alice)), 0);
        QCOMPARE(countChecked(controller.loadPlayChecks(bob)), 0);
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
