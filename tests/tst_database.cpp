#include <QtTest>
#include "../src/store/database.h"
#include "../src/core/jsoncodec.h"
#include <QTemporaryDir>

class DatabaseTest : public QObject
{
    Q_OBJECT
private slots:
    void persistProject()
    {
        QTemporaryDir dir;
        store::Database db;
        QVERIFY(db.open(dir.path() + "/t.db"));
        auto p = core::JsonCodec::defaultProject();
        p.title = "Persist";
        QVERIFY(db.upsertProject(p));
        const auto loaded = db.getProject(p.id);
        QVERIFY(loaded.has_value());
        QCOMPARE(QString::fromStdString(loaded->title), QStringLiteral("Persist"));
    }
};

QTEST_MAIN(DatabaseTest)
#include "tst_database.moc"
