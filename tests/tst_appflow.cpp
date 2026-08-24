#include "../src/app/appcontroller.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QSignalSpy>
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
        const int rowsN = controller.gridRows();
        const int colsN = controller.gridCols();
        for (int r = 0; r < rowsN; ++r) {
            QVariantList row;
            for (int c = 0; c < colsN; ++c)
                row.append(false);
            // append(QVariantList) aplatit — encapsuler comme checksToVariant.
            checks.append(QVariant(row));
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
        QVERIFY(controller.gageMode());
        QVERIFY2(controller.cases().size() >= 40, "assez de phrases pour varier");
        QVERIFY2(controller.gages().size() >= 10, "assez de gages (variantes)");
        QVERIFY(controller.grids().size() >= 2);
        QCOMPARE(controller.grids().size(), controller.players().size());
        const auto grid0 = controller.grids()[0].toMap();
        const auto cells = grid0.value(QStringLiteral("cells")).toList();
        QCOMPARE(cells.size(), controller.gridSize());
        QVERIFY(cells[0].toList().size() >= controller.gridSize());

        // Les grilles des joueurs doivent différer (pool > cases jouables).
        auto flattenLabels = [](const QVariantMap& g) {
            QStringList out;
            const auto rows = g.value(QStringLiteral("cells")).toList();
            for (const auto& rowV : rows) {
                for (const auto& cellV : rowV.toList()) {
                    const auto m = cellV.toMap();
                    if (m.value(QStringLiteral("isFree")).toBool())
                        continue;
                    out << m.value(QStringLiteral("label")).toString();
                }
            }
            out.sort();
            return out;
        };
        const auto labels0 = flattenLabels(grid0);
        const auto labels1 = flattenLabels(controller.grids()[1].toMap());
        QVERIFY2(labels0 != labels1, "grilles joueurs distinctes");

        // Swap deux cases non-FREE (0,0) et (0,1) si possible
        const auto before = cells[0].toList()[0].toMap().value(QStringLiteral("label")).toString();
        controller.swapGridCells(0, 0, 0, 0, 1);
        const auto afterRows = controller.grids()[0].toMap().value(QStringLiteral("cells")).toList();
        const auto after01 = afterRows[0].toList()[1].toMap().value(QStringLiteral("label")).toString();
        QCOMPARE(after01, before);

        controller.moveGrid(0, 1);
        QCOMPARE(controller.grids().size(), controller.players().size());
    }

    void createProjectOpensSettingsTab()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        const QString id = controller.createProject();
        QVERIFY(!id.isEmpty());
        QCOMPARE(controller.currentProjectId(), id);
        QCOMPARE(controller.lastTab(), 0); // Réglages
    }

    void cloneProjectKeepsGridsAndOpens()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        const QString srcId = controller.seedDemoProject();
        QVERIFY(!srcId.isEmpty());
        QVERIFY(controller.openProject(srcId));
        const int gridCount = controller.grids().size();
        QVERIFY(gridCount >= 2);
        const QString srcTitle = controller.title();

        const QString copyId = controller.cloneProject(srcId);
        QVERIFY(!copyId.isEmpty());
        QVERIFY(copyId != srcId);
        QCOMPARE(controller.currentProjectId(), copyId);
        QCOMPARE(controller.lastTab(), 0);
        QVERIFY(controller.title().contains(QStringLiteral("copie")));
        QCOMPARE(controller.grids().size(), gridCount);
        QVERIFY(controller.title() != srcTitle);
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
        // Même gage pour Alice et Bob → un overlay groupé « Alice et Bob doivent : … »
        // (des combos peuvent aussi partir si toute la grille se coche d'un coup)
        QVariantList gageOverlays;
        for (const auto& ovV : overlays) {
            if (ovV.toMap().value(QStringLiteral("kind")).toString() == QLatin1String("gage"))
                gageOverlays.append(ovV);
        }
        QCOMPARE(gageOverlays.size(), 1);
        const auto ov = gageOverlays[0].toMap();
        const auto players = ov.value(QStringLiteral("players")).toStringList();
        QVERIFY(players.contains(alice));
        QVERIFY(players.contains(bob));
        QCOMPARE(players.size(), 2);
        const QString prompt = ov.value(QStringLiteral("prompt")).toString();
        QVERIFY(prompt.contains(QStringLiteral("doivent")));
        QVERIFY(prompt.contains(alice));
        QVERIFY(prompt.contains(bob));

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

    void toggleDoesNotWipeOtherPlayersUniqueChecks()
    {
        // Régression : applyPlayChecks/save écrasait une grille avec la matrice
        // d'un autre joueur → coches fantômes / décoches. Ici on vérifie que le
        // toggle C++ préserve les coches déjà posées sur des libellés distincts.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        const QString id = controller.createProject();
        QVERIFY(controller.openProject(id));
        controller.setGridSize(2);
        controller.setFreeCenter(false);
        controller.setGageMode(false);
        while (controller.players().size() > 2)
            controller.removePlayer(controller.players().size() - 1);
        while (controller.players().size() < 2)
            controller.addPlayer();
        controller.setPlayerName(0, QStringLiteral("Alice"));
        controller.setPlayerName(1, QStringLiteral("Bob"));
        while (controller.cases().size() > 0)
            controller.removeCase(0);
        controller.addCase(QStringLiteral("SeulAlice"), 1, 100);
        controller.addCase(QStringLiteral("SeulBob"), 1, 100);
        controller.addCase(QStringLiteral("Commun"), 1, 100);
        controller.addCase(QStringLiteral("Autre"), 1, 100);
        QVERIFY2(!controller.generateAll().contains(QStringLiteral("Aucun")), "generate");
        controller.setGridCellLabel(0, 0, 0, QStringLiteral("SeulAlice"), 1);
        controller.setGridCellLabel(0, 0, 1, QStringLiteral("Commun"), 1);
        controller.setGridCellLabel(0, 1, 0, QStringLiteral("Autre"), 1);
        controller.setGridCellLabel(0, 1, 1, QStringLiteral("Autre"), 1);
        controller.setGridCellLabel(1, 0, 0, QStringLiteral("SeulBob"), 1);
        controller.setGridCellLabel(1, 0, 1, QStringLiteral("Commun"), 1);
        controller.setGridCellLabel(1, 1, 0, QStringLiteral("Autre"), 1);
        controller.setGridCellLabel(1, 1, 1, QStringLiteral("Autre"), 1);

        QVERIFY(controller.togglePlayCell(QStringLiteral("Alice"), 0, 0)
                    .value(QStringLiteral("checked")).toBool());
        auto aliceChecks = controller.loadPlayChecks(QStringLiteral("Alice"));
        QVERIFY(aliceChecks[0].toList()[0].toBool()); // SeulAlice
        QVERIFY(!aliceChecks[0].toList()[1].toBool());

        // Bob coche « Commun » → doit cocher Commun chez Alice SANS toucher SeulAlice
        QVERIFY(controller.togglePlayCell(QStringLiteral("Bob"), 0, 1)
                    .value(QStringLiteral("checked")).toBool());
        aliceChecks = controller.loadPlayChecks(QStringLiteral("Alice"));
        QVERIFY2(aliceChecks[0].toList()[0].toBool(), "SeulAlice still checked");
        QVERIFY2(aliceChecks[0].toList()[1].toBool(), "Commun now checked");
        QVERIFY(!aliceChecks[1].toList()[0].toBool());
        QVERIFY(!aliceChecks[1].toList()[1].toBool());

        // savePlayChecks plat rejeté
        controller.savePlayChecks(QStringLiteral("Alice"), QVariantList{ true, false, true, false });
        aliceChecks = controller.loadPlayChecks(QStringLiteral("Alice"));
        QVERIFY2(aliceChecks[0].toList()[0].toBool(), "flat save ignored");
    }

    void weightedGagePickIgnoresZeroRate()
    {
        // Deux gages même n° : rate 100 vs 0 → toujours le 100 %.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        const QString id = controller.createProject();
        QVERIFY(controller.openProject(id));
        controller.setGridSize(2);
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
        controller.addGage(QStringLiteral("Gage sur"), 0, 1, 100);
        controller.addGage(QStringLiteral("Gage jamais"), 0, 1, 0);
        while (controller.cases().size() > 0)
            controller.removeCase(0);
        for (int i = 0; i < 4; ++i)
            controller.addCase(QStringLiteral("Scene ") + QString::number(i), 1, 100);
        QVERIFY2(!controller.generateAll().contains(QStringLiteral("Aucun")), "generate");
        controller.setGridCellLabel(0, 0, 0, QStringLiteral("Scene test"), 1);

        const QString alice = QStringLiteral("Alice");
        // Matrice vide valide pour éviter un JSON corrompu résiduel.
        QVariantList empty;
        for (int r = 0; r < 2; ++r) {
            QVariantList row;
            for (int c = 0; c < 2; ++c)
                row.append(false);
            empty.append(QVariant(row));
        }
        controller.savePlayChecks(alice, empty);
        controller.savePlayChecks(QStringLiteral("Bob"), empty);

        const auto result = controller.togglePlayCell(alice, 0, 0);
        QVERIFY(result.value(QStringLiteral("checked")).toBool());
        const auto overlays = result.value(QStringLiteral("overlays")).toList();
        QVERIFY(!overlays.isEmpty());
        const auto ov = overlays[0].toMap();
        QCOMPARE(ov.value(QStringLiteral("kind")).toString(), QStringLiteral("gage"));
        QCOMPARE(ov.value(QStringLiteral("desc")).toString(), QStringLiteral("Gage sur"));
        QCOMPARE(ov.value(QStringLiteral("rate")).toInt(), 100);
        QCOMPARE(ov.value(QStringLiteral("variants")).toInt(), 2);

        controller.updateGage(1, QStringLiteral("Gage risque"), 0, 1, 10);
        QCOMPARE(controller.gages()[1].toMap().value(QStringLiteral("rate")).toInt(), 10);
        QCOMPARE(controller.gages()[1].toMap().value(QStringLiteral("number")).toInt(), 1);
        QCOMPARE(controller.maxGageNumber(), 1);
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

    void scoreboardExportStress()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        const QString id = controller.createProject();
        QVERIFY(controller.openProject(id));
        controller.setTitle(QStringLiteral("Soirée marathon des noms interminables"));
        controller.setDescription(QStringLiteral("Description longue pour vérifier l'export PNG du classement."));
        controller.setGridSize(3);
        controller.setFreeCenter(true);
        controller.setGageMode(false); // score = points, pas n° de cases

        while (controller.players().size() > 0)
            controller.removePlayer(controller.players().size() - 1);

        const QStringList names = {
            QStringLiteral("Jean-Baptiste de la Fontaine-des-Prés"),
            QStringLiteral("Marie-Christine Élizabeth von Habsbourg-Lorraine"),
            QStringLiteral("Alexandre-Maximilien Quatresous-Longnom"),
            QStringLiteral("Bob"),
            QStringLiteral("Charlotte-Amélie du Château-Neuf-sur-Loire"),
            QStringLiteral("Dimitri « Le Magnifique » Petrovitch"),
            QStringLiteral("Éléonore-Françoise de Montmorency-Luxembourg"),
            QStringLiteral("Félix"),
            QStringLiteral("Gwendoline Aphrodite Supercalifragilistic"),
            QStringLiteral("Henriette-Victoire de Saint-Germain-des-Prés"),
            QStringLiteral("Isidore le Très Très Long Nom Qui Déborde"),
            QStringLiteral("Jeanne d'Arc-en-ciel-multicolore"),
        };
        for (const QString& n : names) {
            controller.addPlayer();
            controller.setPlayerName(controller.players().size() - 1, n);
        }
        QCOMPARE(controller.players().size(), names.size());

        while (controller.cases().size() > 0)
            controller.removeCase(0);
        // Gros points pour des scores absurdes
        const int pts[] = { 1000, 2500, 5000, 7500, 9999, 12000, 15000, 20000, 1 };
        for (int p : pts)
            controller.addCase(QStringLiteral("Phrase %1").arg(p), p, 100);

        const QString genMsg = controller.generateAll();
        QVERIFY2(!genMsg.contains(QStringLiteral("Aucun")), qPrintable(genMsg));
        QCOMPARE(controller.grids().size(), names.size());

        const int N = controller.gridSize();
        auto makeChecks = [N](int checkedTarget) {
            QVariantList checks;
            int left = checkedTarget;
            for (int r = 0; r < N; ++r) {
                QVariantList row;
                for (int c = 0; c < N; ++c) {
                    const bool mark = left > 0;
                    if (mark)
                        --left;
                    row.append(mark);
                }
                checks.append(QVariant(row));
            }
            return checks;
        };

        // Scores croissants + un FULL en tête
        for (int i = 0; i < names.size(); ++i) {
            const int checked = (i == 0) ? N * N : qMin(N * N - 1, i + 1);
            // Booster les points des cases du joueur i pour un score énorme
            for (int r = 0; r < N; ++r) {
                for (int c = 0; c < N; ++c) {
                    if (r == N / 2 && c == N / 2 && controller.freeCenter() && N % 2 == 1)
                        continue;
                    controller.setGridCellLabel(i, r, c,
                        QStringLiteral("Mega %1-%2-%3").arg(i).arg(r).arg(c),
                        100000 + i * 7777 + r * 100 + c);
                }
            }
            controller.savePlayChecks(names[i], makeChecks(checked));
        }

        const QVariantList board = controller.playScoreboard();
        QCOMPARE(board.size(), names.size());
        // Le premier (FULL) doit être en tête
        QCOMPARE(board[0].toMap().value(QStringLiteral("player")).toString(), names[0]);
        QVERIFY(board[0].toMap().value(QStringLiteral("full")).toBool());
        QVERIFY(board[0].toMap().value(QStringLiteral("score")).toInt() > 100000);
        // Tri : full d'abord, puis score décroissant
        for (int i = 1; i < board.size(); ++i) {
            const auto a = board[i - 1].toMap();
            const auto b = board[i].toMap();
            if (a.value(QStringLiteral("full")).toBool() == b.value(QStringLiteral("full")).toBool())
                QVERIFY(a.value(QStringLiteral("score")).toInt()
                        >= b.value(QStringLiteral("score")).toInt());
        }

        const QString pngPath = qEnvironmentVariableIsSet("BINGO_SCOREBOARD_OUT")
            ? QString::fromLocal8Bit(qgetenv("BINGO_SCOREBOARD_OUT"))
            : (tmp.path() + QStringLiteral("/scores-stress.png"));
        QVERIFY2(controller.exportScoreboardPng(pngPath), "exportScoreboardPng");
        QVERIFY(QFileInfo::exists(pngPath));
        QVERIFY(QFileInfo(pngPath).size() > 2000);

        QImage img(pngPath);
        QVERIFY2(!img.isNull(), "PNG lisible");
        QCOMPARE(img.width(), 1000);
        QVERIFY(img.height() > 500);

        const QString previewUrl = controller.prepareScoreboardPreview();
        QVERIFY2(!previewUrl.isEmpty(), "prepareScoreboardPreview url");
        QVERIFY(previewUrl.startsWith(QStringLiteral("file:")));
        QVERIFY(!controller.scoreboardPreviewUrl().isEmpty());
        QVERIFY(controller.scoreboardPreviewRevision() >= 1);
        // Le fichier cache doit exister (sans le #r= de cache-bust).
        const QString previewPath = QUrl(previewUrl).toLocalFile();
        QVERIFY2(QFileInfo::exists(previewPath), "preview file");
        QVERIFY(QFileInfo(previewPath).size() > 2000);
    }

    void announcesWinnerOnFullGrid()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        const QString id = controller.createProject();
        QVERIFY(controller.openProject(id));
        controller.setGageMode(false);
        controller.setGridSize(2);
        controller.setFreeCenter(false);

        while (controller.players().size() > 0)
            controller.removePlayer(controller.players().size() - 1);
        controller.addPlayer();
        controller.setPlayerName(0, QStringLiteral("Alice"));
        controller.addPlayer();
        controller.setPlayerName(1, QStringLiteral("Bob"));

        while (controller.cases().size() > 0)
            controller.removeCase(0);
        controller.addCase(QStringLiteral("A"), 10, 100);
        controller.addCase(QStringLiteral("B"), 20, 100);
        controller.addCase(QStringLiteral("C"), 30, 100);
        controller.addCase(QStringLiteral("D"), 40, 100);

        QVERIFY2(!controller.generateAll().contains(QStringLiteral("Aucun")), "generate");
        QCOMPARE(controller.grids().size(), 2);

        // Libellés distincts : la sync par texte ne doit pas compléter Bob.
        const QStringList aliceCells = {
            QStringLiteral("Alice-1"), QStringLiteral("Alice-2"),
            QStringLiteral("Alice-3"), QStringLiteral("Alice-4")
        };
        const QStringList bobCells = {
            QStringLiteral("Bob-1"), QStringLiteral("Bob-2"),
            QStringLiteral("Bob-3"), QStringLiteral("Bob-4")
        };
        int k = 0;
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 2; ++c) {
                controller.setGridCellLabel(0, r, c, aliceCells[k], 10 + k);
                controller.setGridCellLabel(1, r, c, bobCells[k], 10 + k);
                ++k;
            }
        }

        // Remplir la grille d'Alice case par case — la dernière doit annoncer le gagnant.
        QVariantMap last;
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 2; ++c) {
                last = controller.togglePlayCell(QStringLiteral("Alice"), r, c);
                QVERIFY(last.value(QStringLiteral("checked")).toBool());
            }
        }
        QVERIFY2(last.value(QStringLiteral("justCompleted")).toBool(), "justCompleted");
        const auto newWinners = last.value(QStringLiteral("newWinners")).toList();
        QCOMPARE(newWinners.size(), 1);
        QCOMPARE(newWinners[0].toMap().value(QStringLiteral("player")).toString(),
                 QStringLiteral("Alice"));
        QVERIFY(last.value(QStringLiteral("winners")).toList().contains(QStringLiteral("Alice")));
        QVERIFY(last.value(QStringLiteral("gridFull")).toBool());

        // Décocher annule le statut gagnant
        const auto off = controller.togglePlayCell(QStringLiteral("Alice"), 0, 0);
        QVERIFY(!off.value(QStringLiteral("checked")).toBool());
        QVERIFY(!off.value(QStringLiteral("gridFull")).toBool());
        QVERIFY(off.value(QStringLiteral("winners")).toList().isEmpty());
        QVERIFY(off.value(QStringLiteral("newWinners")).toList().isEmpty());

        // Re-compléter : playWinnersTriggered (toast / panneau pour tout le monde).
        QSignalSpy winSpy(&controller, &app::AppController::playWinnersTriggered);
        QVariantMap again;
        for (int r = 0; r < 2; ++r) {
            for (int c = 0; c < 2; ++c) {
                const auto cur = controller.loadPlayChecks(QStringLiteral("Alice"));
                if (cur.size() > r && cur[r].toList().size() > c
                        && cur[r].toList()[c].toBool())
                    continue;
                again = controller.togglePlayCell(QStringLiteral("Alice"), r, c);
            }
        }
        QVERIFY(again.value(QStringLiteral("justCompleted")).toBool());
        QVERIFY2(winSpy.count() >= 1, "playWinnersTriggered");
        const auto winArgs = winSpy.takeLast();
        QCOMPARE(winArgs.size(), 2);
        QCOMPARE(winArgs[0].toList().size(), 1);
        QCOMPARE(winArgs[0].toList()[0].toMap().value(QStringLiteral("player")).toString(),
                 QStringLiteral("Alice"));
        QVERIFY(!winArgs[1].toList().isEmpty());
    }

    void joinAndLeaveSharedProject()
    {
        QTemporaryDir hostDir;
        QTemporaryDir guestDir;
        QVERIFY(hostDir.isValid() && guestDir.isValid());

        qputenv("XDG_DATA_HOME", hostDir.path().toUtf8());
        app::AppController host;
        QVERIFY(host.init());
        while (host.projects()->rowCount() > 0)
            host.deleteProject(host.projects()->idAt(0));

        const QString id = host.createProject();
        QVERIFY(!id.isEmpty());
        host.setTitle(QStringLiteral("Soirée partagée"));
        const QString uri1 = host.buildShareUrl();
        QVERIFY2(!uri1.isEmpty(), "join URI");
        QVERIFY(uri1.startsWith(QStringLiteral("openbingo://join/")));
        QVERIFY(host.isProjectShared(id));
        // Même URI au 2e appel : la clé ne doit pas être régénérée.
        const QString uri2 = host.buildShareUrl();
        QCOMPARE(uri2, uri1);

        qputenv("XDG_DATA_HOME", guestDir.path().toUtf8());
        app::AppController guest;
        QVERIFY(guest.init());
        while (guest.projects()->rowCount() > 0)
            guest.deleteProject(guest.projects()->idAt(0));

        QVERIFY2(guest.joinProjectUri(uri1), "joinProjectUri");
        QCOMPARE(guest.currentProjectId(), id);
        QCOMPARE(guest.title(), QStringLiteral("Soirée partagée"));
        QVERIFY(guest.isProjectShared(id));
        // Avant sync Nostr : stub sans contenu (le titre vient de l'URI).
        QCOMPARE(guest.cases().size(), 0);
        QCOMPARE(guest.players().size(), 0);

        guest.leaveProject(id);
        QVERIFY(guest.projects()->rowCount() == 0 || !guest.isProjectShared(id));
        QVERIFY(guest.currentProjectId().isEmpty() || guest.currentProjectId() != id);

        // L'hôte garde toujours sa partie partagée.
        QVERIFY(host.isProjectShared(id));
        QVERIFY(host.projects()->rowCount() >= 1);
    }

    // Contrat CSV : import phrases → gridsDirty, grilles non vidées.
    void importPhrasesCsvMarksDirtyWithoutWipingGrids()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_DATA_HOME", tmp.path().toUtf8());

        app::AppController controller;
        QVERIFY(controller.init());
        while (controller.projects()->rowCount() > 0)
            controller.deleteProject(controller.projects()->idAt(0));

        const QString id = controller.createProject();
        QVERIFY(!id.isEmpty());
        controller.setGridSize(3);
        while (controller.players().size() > 2)
            controller.removePlayer(controller.players().size() - 1);
        while (controller.players().size() < 2)
            controller.addPlayer();
        while (controller.cases().size() > 0)
            controller.removeCase(0);
        for (int i = 0; i < 9; ++i)
            controller.addCase(QStringLiteral("Seed %1").arg(i), 1, 100);

        const QString genMsg = controller.generateAll();
        QVERIFY2(!genMsg.contains(QStringLiteral("Aucun")), qPrintable(genMsg));
        const int gridsBefore = controller.grids().size();
        QVERIFY(gridsBefore >= 1);
        QVERIFY(!controller.gridsDirty());

        const QString csv = QStringLiteral(
            "label,points,rate\n"
            "Importée CSV,2,80\n"
            ",1,50\n"
            "Autre phrase,1,60\n");
        const QVariantMap result = controller.importPhrasesCsvText(csv, false);
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(), "import ok");
        QCOMPARE(result.value(QStringLiteral("added")).toInt(), 2);
        QVERIFY(result.value(QStringLiteral("skipped")).toInt() >= 1);

        QCOMPARE(controller.grids().size(), gridsBefore);
        QVERIFY2(controller.gridsDirty(), "gridsDirty after CSV import");
        QVERIFY(controller.cases().size() >= 11);

        const QString exported = controller.exportPhrasesCsvText();
        QVERIFY(exported.contains(QStringLiteral("Importée CSV")));
        QVERIFY(exported.startsWith(QStringLiteral("label,points,rate")));
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
