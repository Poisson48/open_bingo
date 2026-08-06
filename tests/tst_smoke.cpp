#include <QtTest>

class SmokeTest : public QObject
{
    Q_OBJECT
private slots:
    void passes() { QVERIFY(true); }
};

QTEST_MAIN(SmokeTest)
#include "tst_smoke.moc"
