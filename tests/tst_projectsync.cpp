#include "../src/app/projectsync.h"
#include "../src/core/jsoncodec.h"
#include "../src/core/pairing.h"
#include "../src/net/crypto.h"
#include "../src/net/nostr.h"
#include "../src/net/relaypool.h"
#include "../src/store/database.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

class ProjectSyncTest : public QObject
{
    Q_OBJECT
private slots:
    void joinStubHasZeroUpdatedAt()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        store::Database db;
        QVERIFY(db.open(dir.path() + QStringLiteral("/t.db")));

        auto key = net::generateListKey();
        QCOMPARE(key.size(), size_t(32));
        const std::string id = "proj_join_stub";
        const auto uri = core::buildJoinUri(id, key, "Soirée bingo");

        net::RelayPool pool;
        app::ProjectSync sync;
        sync.init(&db, &pool, QStringLiteral("dev"));

        const QString joined = sync.joinFromUri(QString::fromStdString(uri));
        QCOMPARE(joined, QString::fromStdString(id));

        const auto p = db.getProject(id);
        QVERIFY(p);
        QCOMPARE(p->title, std::string("Soirée bingo"));
        QCOMPARE(p->updatedAt, int64_t(0));
        QVERIFY(p->cases.empty());
        QVERIFY(p->grids.empty());
        QVERIFY(p->players.empty());
    }

    void remoteSnapshotBeatsJoinStub()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        store::Database db;
        QVERIFY(db.open(dir.path() + QStringLiteral("/t.db")));

        auto key = net::generateListKey();
        const std::string id = "proj_sync_content";
        const auto tag = net::deriveChannelTag(key);

        core::Project host = core::JsonCodec::defaultProject();
        host.id = id;
        host.title = "Soirée bingo";
        host.updatedAt = 1'000; // plus ancien que « maintenant »
        host.cases = { { "Phrase A", 2, 100 }, { "Phrase B", 3, 100 } };
        host.players = { { "Alice" }, { "Bob" } };

        // Stub invité (comme joinFromUri corrigé)
        core::Project stub;
        stub.id = id;
        stub.title = "Soirée bingo";
        stub.updatedAt = 0;
        db.upsertProject(stub);
        db.setSyncKey(id, key);

        net::RelayPool pool;
        app::ProjectSync sync;
        sync.init(&db, &pool, QStringLiteral("dev"));

        net::NostrEvent ev;
        ev.kind = 4545;
        ev.created_at = 42;
        ev.tags = QJsonArray{ QJsonArray{ QStringLiteral("t"), QString::fromStdString(tag) } };
        ev.content = QString::fromStdString(
            net::encryptPayload(key, tag, core::JsonCodec::projectToJson(host, false)));
        QVERIFY(net::signEvent(ev, net::deriveNostrSeed(key)));

        sync.handleRelayEvent(ev);

        const auto got = db.getProject(id);
        QVERIFY(got);
        QCOMPARE(got->cases.size(), size_t(2));
        QCOMPARE(got->cases[0].label, std::string("Phrase A"));
        QCOMPARE(got->players.size(), size_t(2));
        QCOMPARE(got->players[0].name, std::string("Alice"));
        QCOMPARE(got->updatedAt, int64_t(1'000));
    }

    void staleEmptyStubIsResetOnRejoin()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        store::Database db;
        QVERIFY(db.open(dir.path() + QStringLiteral("/t.db")));

        auto key = net::generateListKey();
        const std::string id = "proj_stale";
        const auto uri = core::buildJoinUri(id, key, "Reset moi");

        // Ancien bug : stub vide avec updatedAt récent → snapshot hôte rejeté.
        core::Project stale;
        stale.id = id;
        stale.title = "Reset moi";
        stale.updatedAt = 9'999'999'999;
        db.upsertProject(stale);

        net::RelayPool pool;
        app::ProjectSync sync;
        sync.init(&db, &pool, QStringLiteral("dev"));
        QVERIFY(!sync.joinFromUri(QString::fromStdString(uri)).isEmpty());

        const auto p = db.getProject(id);
        QVERIFY(p);
        QCOMPARE(p->updatedAt, int64_t(0));
    }

    void contentfulRemoteBeatsHorodatedEmptyStub()
    {
        // Autosave après jointure horodate le stub vide → l'hôte (plus ancien) était rejeté.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        store::Database db;
        QVERIFY(db.open(dir.path() + QStringLiteral("/t.db")));

        auto key = net::generateListKey();
        const std::string id = "proj_empty_beats";
        const auto tag = net::deriveChannelTag(key);

        core::Project host = core::JsonCodec::defaultProject();
        host.id = id;
        host.title = "Soirée";
        host.updatedAt = 1'000;
        host.cases = { { "Phrase A", 2, 100 } };
        host.players = { { "Alice" } };

        core::Project stub;
        stub.id = id;
        stub.title = "Soirée";
        stub.updatedAt = 9'999'999'999; // plus récent que l'hôte
        db.upsertProject(stub);
        db.setSyncKey(id, key);

        net::RelayPool pool;
        app::ProjectSync sync;
        sync.init(&db, &pool, QStringLiteral("dev"));

        net::NostrEvent ev;
        ev.kind = 4545;
        ev.created_at = 42;
        ev.tags = QJsonArray{ QJsonArray{ QStringLiteral("t"), QString::fromStdString(tag) } };
        ev.content = QString::fromStdString(
            net::encryptPayload(key, tag, core::JsonCodec::projectToJson(host, false)));
        QVERIFY(net::signEvent(ev, net::deriveNostrSeed(key)));

        sync.handleRelayEvent(ev);

        const auto got = db.getProject(id);
        QVERIFY(got);
        QCOMPARE(got->cases.size(), size_t(1));
        QCOMPARE(got->players.size(), size_t(1));
        QCOMPARE(got->updatedAt, int64_t(1'000));
        QVERIFY(db.isEventSeen(ev.id.toStdString()));
    }

    void lwwRejectDoesNotMarkEventSeen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        store::Database db;
        QVERIFY(db.open(dir.path() + QStringLiteral("/t.db")));

        auto key = net::generateListKey();
        const std::string id = "proj_lww_seen";
        const auto tag = net::deriveChannelTag(key);

        core::Project local = core::JsonCodec::defaultProject();
        local.id = id;
        local.title = "Local plein";
        local.updatedAt = 5'000;
        local.cases = { { "Chez moi", 1, 100 } };
        local.players = { { "Bob" } };
        db.upsertProject(local);
        db.setSyncKey(id, key);

        core::Project remote = local;
        remote.updatedAt = 1'000; // plus ancien → rejet LWW
        remote.cases = { { "Chez l'hôte", 1, 100 } };

        net::RelayPool pool;
        app::ProjectSync sync;
        sync.init(&db, &pool, QStringLiteral("dev"));

        net::NostrEvent ev;
        ev.kind = 4545;
        ev.created_at = 42;
        ev.tags = QJsonArray{ QJsonArray{ QStringLiteral("t"), QString::fromStdString(tag) } };
        ev.content = QString::fromStdString(
            net::encryptPayload(key, tag, core::JsonCodec::projectToJson(remote, false)));
        QVERIFY(net::signEvent(ev, net::deriveNostrSeed(key)));

        sync.handleRelayEvent(ev);

        QCOMPARE(db.getProject(id)->cases[0].label, std::string("Chez moi"));
        QVERIFY(!db.isEventSeen(ev.id.toStdString()));
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ProjectSyncTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_projectsync.moc"
