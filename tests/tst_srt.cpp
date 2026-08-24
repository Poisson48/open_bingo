#include <QtTest>

#include "../src/core/phraseextract.h"
#include "../src/core/srtcodec.h"

#include <string>

#ifndef OPENBINGO_FIXTURES_DIR
#  define OPENBINGO_FIXTURES_DIR "."
#endif

class SrtTest : public QObject
{
    Q_OBJECT
private slots:
    void parseSampleSdhFile()
    {
        const std::string path = std::string(OPENBINGO_FIXTURES_DIR) + "/sample_sdh.srt";
        const auto r = core::parseSrtFile(path);
        QVERIFY2(r.errors.empty(), r.errors.empty() ? "" : r.errors.front().c_str());
        QCOMPARE(static_cast<int>(r.cues.size()), 4);

        QCOMPARE(r.cues[0].startMs, 1000);
        QCOMPARE(r.cues[0].endMs, 4000);
        QCOMPARE(r.cues[0].plain, std::string("On ne parle pas de Fight Club."));
        QVERIFY(!r.cues[0].likelySfx);

        QCOMPARE(r.cues[1].plain, std::string("[Bruit de porte]"));
        QVERIFY(r.cues[1].likelySfx);

        QCOMPARE(r.cues[2].plain, std::string("Je vois des gens morts."));
        QVERIFY(!r.cues[2].likelySfx);

        QCOMPARE(r.cues[3].text, std::string("<i>Hasta la vista, baby</i>"));
        QCOMPARE(r.cues[3].plain, std::string("Hasta la vista, baby"));
        QVERIFY(!r.cues[3].likelySfx);
    }

    void bomAndMultiline()
    {
        const std::string srt =
            "\xEF\xBB\xBF"
            "1\n"
            "00:00:01,000 --> 00:00:03,000\n"
            "Ligne une\n"
            "Ligne deux\n"
            "\n"
            "2\n"
            "00:00:04,500 --> 00:00:05,000\n"
            "(Musique)\n";

        const auto r = core::parseSrt(srt);
        QVERIFY(r.errors.empty());
        QCOMPARE(static_cast<int>(r.cues.size()), 2);
        QCOMPARE(r.cues[0].plain, std::string("Ligne une Ligne deux"));
        QVERIFY(r.cues[0].text.find('\n') != std::string::npos);
        QVERIFY(r.cues[1].likelySfx);
    }

    void stripHtmlAndAss()
    {
        QCOMPARE(core::cuePlainText("<b>Hello</b> {\\an8}world"), std::string("Hello world"));
        QCOMPARE(core::cuePlainText("{\\i1}italic{\\i0}"), std::string("italic"));
    }

    void parseBasicVtt()
    {
        const std::string vtt =
            "WEBVTT\n"
            "\n"
            "00:00:01.000 --> 00:00:02.500\n"
            "Hello VTT\n"
            "\n"
            "2\n"
            "00:00:03.000 --> 00:00:04.000 align:start\n"
            "[Door slam]\n";

        const auto r = core::parseSrt(vtt);
        QVERIFY(r.errors.empty());
        QCOMPARE(static_cast<int>(r.cues.size()), 2);
        QCOMPARE(r.cues[0].startMs, 1000);
        QCOMPARE(r.cues[0].plain, std::string("Hello VTT"));
        QVERIFY(r.cues[1].likelySfx);
    }

    void suggestSkipsSfxMaxLenDedupe()
    {
        std::vector<core::Cue> cues = {
            {0, 1, "a", "Hello World", false},
            {0, 1, "b", "[noise]", true},
            {0, 1, "c", "hello world", false},
            {0, 1, "d", std::string(70, 'x'), false},
            {0, 1, "e", "Short", false},
        };

        core::PhraseSuggestOptions opt;
        opt.skipSfx = true;
        opt.maxLen = 60;
        opt.dedupe = true;
        opt.maxPhrases = 80;

        const auto phrases = core::suggestBingoPhrases(cues, opt);
        QCOMPARE(static_cast<int>(phrases.size()), 2);
        QCOMPARE(phrases[0], std::string("Hello World"));
        QCOMPARE(phrases[1], std::string("Short"));
    }

    void missingFileErrors()
    {
        const auto r = core::parseSrtFile("/no/such/openbingo_srt_fixture.srt");
        QVERIFY(!r.errors.empty());
        QVERIFY(r.cues.empty());
    }
};

QTEST_MAIN(SrtTest)
#include "tst_srt.moc"
