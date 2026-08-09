#include "../src/app/projectsync.h"
#include "../src/core/jsoncodec.h"
#include "../src/core/pairing.h"
#include "../src/core/sharecodec.h"
#include "../src/net/crypto.h"
#include "../src/net/nostr.h"
#include "../src/net/relaypool.h"
#include "../src/store/database.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
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
        {
            QJsonArray tagT;
            tagT.append(QStringLiteral("t"));
            tagT.append(QString::fromStdString(tag));
            ev.tags.append(tagT);
        }
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

    void compressedRemoteSnapshotIsAccepted()
    {
        // Gros bingo : plaintext compressé (z:) avant chiffrement — format sync v2.0.33+.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        store::Database db;
        QVERIFY(db.open(dir.path() + QStringLiteral("/t.db")));

        auto key = net::generateListKey();
        const std::string id = "proj_sync_z";
        const auto tag = net::deriveChannelTag(key);

        core::Project host = core::JsonCodec::defaultProject();
        host.id = id;
        host.title = "Cinéma";
        host.updatedAt = 2'000;
        for (int i = 0; i < 40; ++i)
            host.cases.push_back({ "Phrase longue numéro " + std::to_string(i), 1, 100 });
        host.players = { { "A" }, { "B" }, { "C" } };

        core::Project stub;
        stub.id = id;
        stub.title = "Cinéma";
        stub.updatedAt = 0;
        db.upsertProject(stub);
        db.setSyncKey(id, key);

        net::RelayPool pool;
        app::ProjectSync sync;
        sync.init(&db, &pool, QStringLiteral("dev"));

        const std::string packed =
            core::ShareCodec::compress(core::JsonCodec::projectToJson(host, false));
        QVERIFY(packed.rfind("z:", 0) == 0);

        net::NostrEvent ev;
        ev.kind = 4545;
        ev.created_at = 42;
        {
            QJsonArray tagT;
            tagT.append(QStringLiteral("t"));
            tagT.append(QString::fromStdString(tag));
            ev.tags.append(tagT);
        }
        ev.content = QString::fromStdString(net::encryptPayload(key, tag, packed));
        QVERIFY(net::signEvent(ev, net::deriveNostrSeed(key)));

        sync.handleRelayEvent(ev);

        const auto got = db.getProject(id);
        QVERIFY(got);
        QCOMPARE(got->cases.size(), size_t(40));
        QCOMPARE(got->players.size(), size_t(3));
        QCOMPARE(got->title, std::string("Cinéma"));
    }

    void compressedFatSnapshotStaysUnderRelayLimit()
    {
        // Sans compression, ~100 Ko JSON → event ~127 Ko rejeté (« too large »).
        // Avec z: + chiffrement, même un bingo absurde doit rester << 100 Ko filaire.
        core::Project p = core::JsonCodec::defaultProject();
        p.id = "proj_fat_wire";
        p.title = "Stress";
        p.gageMode = true;
        p.gridSize = 5;
        p.cases.clear();
        p.players.clear();
        p.grids.clear();
        p.gages.clear();
        const std::string pad(120, 'x');
        for (int i = 0; i < 80; ++i)
            p.cases.push_back({ "Phrase #" + std::to_string(i) + " " + pad, 1, 100 });
        for (int i = 0; i < 16; ++i)
            p.players.push_back({ "Joueur_" + std::to_string(i) });
        const std::string gpad(150, 'g');
        for (int i = 0; i < 60; ++i) {
            core::Gage g;
            g.description = "Gage #" + std::to_string(i) + " " + gpad;
            g.hp = 5;
            g.number = 1 + (i % 8);
            g.rate = 100;
            p.gages.push_back(g);
        }
        for (const auto& pl : p.players) {
            core::PlayerGrid g;
            g.player = pl.name;
            g.cells.assign(5, std::vector<core::GridCell>(5));
            for (int r = 0; r < 5; ++r) {
                for (int c = 0; c < 5; ++c) {
                    const auto& src = p.cases[(r * 5 + c) % p.cases.size()];
                    g.cells[r][c].label = src.label;
                    g.cells[r][c].points = src.points;
                    g.cells[r][c].rate = src.rate;
                    g.cells[r][c].gage = p.gages[(r + c) % p.gages.size()].description;
                    g.cells[r][c].gageHP = 5;
                }
            }
            p.grids.push_back(g);
        }

        const std::string json = core::JsonCodec::projectToJson(p, false);
        QVERIFY(json.size() > 100000); // sans compression, au-delà de la limite relais
        const std::string packed = core::ShareCodec::compress(json);
        QVERIFY(packed.rfind("z:", 0) == 0);

        auto key = net::generateListKey();
        const auto tag = net::deriveChannelTag(key);
        net::NostrEvent ev;
        ev.kind = 4545;
        ev.created_at = 1;
        {
            QJsonArray tagT;
            tagT.append(QStringLiteral("t"));
            tagT.append(QString::fromStdString(tag));
            ev.tags.append(tagT);
        }
        ev.content = QString::fromStdString(net::encryptPayload(key, tag, packed));
        QVERIFY(net::signEvent(ev, net::deriveNostrSeed(key)));

        const auto wire = QJsonDocument(ev.toJson()).toJson(QJsonDocument::Compact).size();
        QVERIFY2(wire < 100 * 1024,
                 qPrintable(QStringLiteral("wire=%1 json=%2 packed=%3")
                                .arg(wire)
                                .arg(qsizetype(json.size()))
                                .arg(qsizetype(packed.size()))));
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
        {
            QJsonArray tagT;
            tagT.append(QStringLiteral("t"));
            tagT.append(QString::fromStdString(tag));
            ev.tags.append(tagT);
        }
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
        {
            QJsonArray tagT;
            tagT.append(QStringLiteral("t"));
            tagT.append(QString::fromStdString(tag));
            ev.tags.append(tagT);
        }
        ev.content = QString::fromStdString(
            net::encryptPayload(key, tag, core::JsonCodec::projectToJson(remote, false)));
        QVERIFY(net::signEvent(ev, net::deriveNostrSeed(key)));

        sync.handleRelayEvent(ev);

        QCOMPARE(db.getProject(id)->cases[0].label, std::string("Chez moi"));
        QVERIFY(!db.isEventSeen(ev.id.toStdString()));
    }

    void publishedEventTagsAreNestedArrays()
    {
        // Régression Android/Clang : QJsonArray{ QJsonArray{…} } aplatit les tags
        // → relais strfry « tag in tags field was not an array ».
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        store::Database db;
        QVERIFY(db.open(dir.path() + QStringLiteral("/t.db")));

        core::Project p = core::JsonCodec::defaultProject();
        p.id = "proj_tags_shape";
        p.title = "Tags";
        p.updatedAt = 1;
        p.cases = { { "A", 1, 100 } };
        p.players = { { "Alice" } };
        db.upsertProject(p);

        net::RelayPool pool;
        app::ProjectSync sync;
        sync.init(&db, &pool, QStringLiteral("dev"));
        sync.enableSharing(QString::fromStdString(p.id));

        const auto pending = db.outboxPeekAll();
        QVERIFY(!pending.empty());
        const auto doc = QJsonDocument::fromJson(
            QByteArray::fromStdString(pending.front().content));
        QVERIFY(doc.isObject());
        const QJsonArray tags = doc.object().value(QStringLiteral("tags")).toArray();
        QCOMPARE(tags.size(), 1);
        QVERIFY(tags.at(0).isArray());
        const QJsonArray tag0 = tags.at(0).toArray();
        QCOMPARE(tag0.size(), 2);
        QCOMPARE(tag0.at(0).toString(), QStringLiteral("t"));
        QVERIFY(!tag0.at(1).toString().isEmpty());
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
