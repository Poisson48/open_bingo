#include <QtTest>
#include "../src/core/jsoncodec.h"

class JsonTest : public QObject
{
    Q_OBJECT
private slots:
    void roundTrip()
    {
        auto p = core::JsonCodec::defaultProject();
        p.title = "Test Bingo";
        p.cases.push_back({ "Phrase", 3, 80 });
        const auto json = core::JsonCodec::projectToJson(p);
        bool ok = false;
        const auto back = core::JsonCodec::projectFromJson(json, &ok);
        QVERIFY(ok);
        QCOMPARE(back.title, p.title);
        QCOMPARE(static_cast<int>(back.cases.size()), 1);
    }
};

QTEST_MAIN(JsonTest)
#include "tst_json.moc"
