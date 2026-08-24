#include "../src/core/projectcsv.h"

#include <QtTest>

#include <string>

class CsvProjectTest : public QObject
{
    Q_OBJECT
private slots:
    void phrasesRoundTrip()
    {
        core::Project p;
        p.cases = {
            {"Hello, world", 2, 80},
            {"He said \"hi\"", 1, 50},
            {"Plain", 3, 100},
        };
        const std::string csv = core::exportPhrasesCsv(p);
        QVERIFY(csv.find("label,points,rate") == 0);
        QVERIFY(csv.find("\"Hello, world\"") != std::string::npos);
        QVERIFY(csv.find("\"He said \"\"hi\"\"\"") != std::string::npos);

        core::Project back;
        const auto r = core::importPhrasesCsv(back, csv, true);
        QCOMPARE(r.added, 3);
        QCOMPARE(r.skipped, 0);
        QVERIFY(r.errors.empty());
        QCOMPARE(static_cast<int>(back.cases.size()), 3);
        QCOMPARE(back.cases[0].label, std::string("Hello, world"));
        QCOMPARE(back.cases[0].points, 2);
        QCOMPARE(back.cases[0].rate, 80);
        QCOMPARE(back.cases[1].label, std::string("He said \"hi\""));
        QCOMPARE(back.cases[2].label, std::string("Plain"));
    }

    void gagesRoundTrip()
    {
        core::Project p;
        p.gages = {
            {"Boire une gorgée", 5, 1, 100},
            {"Faire 10 pompes", 10, 2, 40},
        };
        const std::string csv = core::exportGagesCsv(p);
        QVERIFY(csv.find("description,number,hp,rate") == 0);

        core::Project back;
        const auto r = core::importGagesCsv(back, csv, true);
        QCOMPARE(r.added, 2);
        QVERIFY(r.errors.empty());
        QCOMPARE(back.gages[0].description, std::string("Boire une gorgée"));
        QCOMPARE(back.gages[0].number, 1);
        QCOMPARE(back.gages[0].hp, 5);
        QCOMPARE(back.gages[0].rate, 100);
        QCOMPARE(back.gages[1].number, 2);
        QCOMPARE(back.gages[1].hp, 10);
    }

    void bomUtf8Accepted()
    {
        const std::string bom = "\xEF\xBB\xBF";
        const std::string text = bom + "label,points,rate\r\n"
                                      "Phrase BOM,2,60\r\n";
        core::Project p;
        const auto r = core::importPhrasesCsv(p, text, true);
        QCOMPARE(r.added, 1);
        QVERIFY(r.errors.empty());
        QCOMPARE(p.cases[0].label, std::string("Phrase BOM"));
        QCOMPARE(p.cases[0].points, 2);
        QCOMPARE(p.cases[0].rate, 60);
    }

    void replaceVsAppend()
    {
        core::Project p;
        p.cases = {{"Old", 1, 50}};
        const std::string csv = "label,points,rate\r\nNew,2,70\r\n";

        auto append = core::importPhrasesCsv(p, csv, false);
        QCOMPARE(append.added, 1);
        QCOMPARE(static_cast<int>(p.cases.size()), 2);
        QCOMPARE(p.cases[0].label, std::string("Old"));
        QCOMPARE(p.cases[1].label, std::string("New"));

        auto repl = core::importPhrasesCsv(p, csv, true);
        QCOMPARE(repl.added, 1);
        QCOMPARE(static_cast<int>(p.cases.size()), 1);
        QCOMPARE(p.cases[0].label, std::string("New"));
    }

    void skipEmptyLabelsAndBlankRows()
    {
        const std::string text =
            "phrase,gage,rate\r\n"
            "Ok,1,50\r\n"
            "\r\n"
            ",2,50\r\n"
            "   ,3,50\r\n"
            "Also ok,4,90\r\n";
        core::Project p;
        const auto r = core::importPhrasesCsv(p, text, true);
        QCOMPARE(r.added, 2);
        QVERIFY(r.skipped >= 2);
        QCOMPARE(static_cast<int>(p.cases.size()), 2);
        QCOMPARE(p.cases[0].label, std::string("Ok"));
        QCOMPARE(p.cases[0].points, 1); // alias gage → points
        QCOMPARE(p.cases[1].label, std::string("Also ok"));
        QCOMPARE(p.cases[1].rate, 90);
    }

    void columnOrderFreeAndRateClamp()
    {
        const std::string text =
            "rate,texte,points\r\n"
            "150,Clamped,2\r\n"
            "-5,Low,1\r\n";
        core::Project p;
        const auto r = core::importPhrasesCsv(p, text, true);
        QCOMPARE(r.added, 2);
        QCOMPARE(p.cases[0].label, std::string("Clamped"));
        QCOMPARE(p.cases[0].rate, 100);
        QCOMPARE(p.cases[1].rate, 0);
    }

    void gagesNumberFloorAndAliases()
    {
        const std::string text =
            "texte,n,hp,rate\r\n"
            "Desc,0,3,200\r\n";
        core::Project p;
        const auto r = core::importGagesCsv(p, text, true);
        QCOMPARE(r.added, 1);
        QCOMPARE(p.gages[0].description, std::string("Desc"));
        QCOMPARE(p.gages[0].number, 1); // number ≥ 1
        QCOMPARE(p.gages[0].hp, 3);
        QCOMPARE(p.gages[0].rate, 100);
    }

    void missingHeaderErrors()
    {
        core::Project p;
        const auto r = core::importPhrasesCsv(p, "noheader\r\nfoo,1,2\r\n", true);
        QCOMPARE(r.added, 0);
        QVERIFY(!r.errors.empty());
        QCOMPARE(static_cast<int>(p.cases.size()), 0);
    }

    void importDoesNotTouchGrids()
    {
        core::Project p;
        p.cases = {{"A", 1, 100}};
        p.grids.resize(1);
        p.grids[0].player = "J1";
        const auto r = core::importPhrasesCsv(
            p, "label,points,rate\r\nB,1,50\r\n", false);
        QCOMPARE(r.added, 1);
        QCOMPARE(static_cast<int>(p.grids.size()), 1);
        QCOMPARE(p.grids[0].player, std::string("J1"));
    }
};

QTEST_APPLESS_MAIN(CsvProjectTest)
#include "tst_csv_project.moc"
