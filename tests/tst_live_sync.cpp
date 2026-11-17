// tst_live_sync.cpp — validation réseau réelle (relais colo-apps).
//
// Deux clients (A = hôte, B = invité), deux bases SQLite, le vrai relais :
//   1. A partage un projet → B rejoint par URI → B reçoit le contenu ;
//   2. B modifie le projet → A reçoit la modification (LWW `updatedAt`) ;
//   3. B coche une case Play → A reçoit les coches ;
//   4. l'outbox des deux clients finit vidée (accusé durable du relais).
//
// Désactivé par défaut (pas de réseau en CI) :
//   OPENBINGO_LIVE_SYNC=1 ./build/tests/tst_live_sync
// ou
//   OPENBINGO_LIVE_SYNC=1 ctest --test-dir build -R live_sync --output-on-failure

#include "../src/app/projectsync.h"
#include "../src/core/jsoncodec.h"
#include "../src/net/relaypool.h"
#include "../src/store/database.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QtTest>

using namespace core;

class LiveSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void twoClientsSyncThroughRealRelay()
    {
        if (qEnvironmentVariable("OPENBINGO_LIVE_SYNC") != QLatin1String("1"))
            QSKIP("Test réseau réel désactivé (OPENBINGO_LIVE_SYNC=1 pour l'activer)");

        QTemporaryDir dirA;
        QTemporaryDir dirB;
        QVERIFY(dirA.isValid());
        QVERIFY(dirB.isValid());

        store::Database dbA;
        store::Database dbB;
        QVERIFY(dbA.open(dirA.path() + QStringLiteral("/a.db")));
        QVERIFY(dbB.open(dirB.path() + QStringLiteral("/b.db")));

        net::RelayPool poolA;
        net::RelayPool poolB;

        app::ProjectSync syncA;
        app::ProjectSync syncB;
        syncA.init(&dbA, &poolA, QStringLiteral("dev-live-a"));
        syncB.init(&dbB, &poolB, QStringLiteral("dev-live-b"));

        poolA.setRelays(net::RelayPool::defaultRelays());
        poolB.setRelays(net::RelayPool::defaultRelays());
        poolA.connectAll();
        poolB.connectAll();
        QTRY_VERIFY_WITH_TIMEOUT(poolA.isOnline(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(poolB.isOnline(), 20000);

        // Projet hôte : contenu non vide (sinon pas de snapshot publié).
        Project host = JsonCodec::defaultProject();
        host.id = "proj_live_" + std::to_string(QDateTime::currentMSecsSinceEpoch());
        host.title = "Bingo live";
        host.updatedAt = QDateTime::currentMSecsSinceEpoch();
        host.players = { { "Alice" }, { "Bob" } };
        host.cases = { { "Phrase live A", 2, 100 }, { "Phrase live B", 3, 100 } };
        dbA.upsertProject(host);
        syncA.enableSharing(QString::fromStdString(host.id));

        const QString uri = syncA.joinUri(QString::fromStdString(host.id),
                                          QString::fromStdString(host.title));
        QVERIFY(!uri.isEmpty());

        const QString joined = syncB.joinFromUri(uri);
        QCOMPARE(joined, QString::fromStdString(host.id));

        // 1. B reçoit le contenu publié par A (snapshot initial / rehost).
        QTRY_VERIFY_WITH_TIMEOUT(
            dbB.getProject(host.id) && !dbB.getProject(host.id)->cases.empty(), 25000);

        // 2. B modifie le projet ; A doit recevoir la version LWW plus récente.
        Project fromB = *dbB.getProject(host.id);
        fromB.title = "Bingo live (modifié par B)";
        fromB.updatedAt = QDateTime::currentMSecsSinceEpoch() + 1000;
        fromB.cases.push_back({ "Phrase ajoutée par B", 1, 100 });
        dbB.upsertProject(fromB);
        syncB.onLocalProjectChange(QString::fromStdString(host.id));

        QTRY_VERIFY_WITH_TIMEOUT(
            dbA.getProject(host.id) && dbA.getProject(host.id)->title
                == "Bingo live (modifié par B)", 25000);

        // 3. B coche une case Play ; A reçoit les coches (bundle playChecks).
        QVERIFY(dbB.savePlayChecks(host.id, "Alice",
                                   "[[true,false],[false,false]]"));
        syncB.onLocalProjectChange(QString::fromStdString(host.id));
        QTRY_VERIFY_WITH_TIMEOUT(
            dbA.getAllPlayChecks(host.id).count("Alice") > 0, 25000);

        // 4. Les accusés du relais vident les outbox (aucune perte, rien en attente).
        QTRY_COMPARE_WITH_TIMEOUT(syncA.pendingChanges(), 0, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(syncB.pendingChanges(), 0, 20000);
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    LiveSyncTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_live_sync.moc"
