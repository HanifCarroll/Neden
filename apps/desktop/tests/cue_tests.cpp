#include "cues.h"
#include <QtTest>

class CueTests : public QObject {
    Q_OBJECT
private slots:
    void webVttKeepsIndependentTimingsAndPlainText() {
        const auto cues = parseSrt("WEBVTT\n\nNOTE ignored\nmetadata\n\nfirst\n00:01.250 --> 00:03.000 "
                                   "align:center\n<i>İstanbul</i>\n\n00:03.000 --> 00:05.100\n日本語\n");
        QCOMPARE(cues.size(), 2);
        QCOMPARE(cues[0].startMs, 1250);
        QCOMPARE(cues[0].endMs, 3000);
        QCOMPARE(cues[0].text, QString("İstanbul"));
        QCOMPARE(subtitlePlainText("<v Ali><c.green>Şüphe</c></v>"), QString("Şüphe"));
        QCOMPARE(cues[1].text, QString("日本語"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt("WEBVTT\n\n00:60.000 --> 01:01.000\nBad"));
        CueModel model;
        model.replace(cues);
        model.setTranslationCues({{1250, 3000, "English", {}}});
        model.setTranslationCues({});
        QVERIFY(model.entries()[0].translation.isEmpty());
    }

    void turkishSearch_data() {
        QTest::addColumn<QString>("query");
        QTest::addColumn<int>("expectedCount");
        for (const auto &query : QStringList{"İSTANBUL", "istanbul", "IŞIK", "ışık", "ILIK", "ılık", "ÇIĞ",
                                             "çığ", "izmir", "ŞÜPHE", "GÖL", "I\u0307STANBUL"}) {
            QTest::newRow(query.toUtf8().constData()) << query << 1;
        }
        QTest::newRow("wrong dotless spelling") << QString("ızmir") << 0;
        QTest::newRow("wrong dotted spelling") << QString("işik") << 0;
        QTest::newRow("accents remain significant") << QString("gol") << 0;
    }
    void turkishSearch() {
        QFETCH(QString, query);
        QFETCH(int, expectedCount);
        const QString original = "İstanbul, İzmir; ışık ve ılık çay. Çığ, göl, şüphe.";
        CueModel model;
        model.setSearchLanguage(CueModel::Turkish);
        model.replace({{0, 1000, original, {}}});
        model.search(query);
        QCOMPARE(model.count(), expectedCount);
        QCOMPARE(model.entries()[0].text, original);
    }
    void turkishPairsStayDistinct_data() {
        QTest::addColumn<QString>("query");
        QTest::addColumn<QList<int>>("expectedIndices");
        QTest::newRow("dotted lowercase") << QString("i") << QList<int>{0, 1};
        QTest::newRow("dotted uppercase") << QString("İ") << QList<int>{0, 1};
        QTest::newRow("dotless lowercase") << QString("ı") << QList<int>{2, 3};
        QTest::newRow("dotless uppercase") << QString("I") << QList<int>{2, 3};
    }
    void turkishPairsStayDistinct() {
        QFETCH(QString, query);
        QFETCH(QList<int>, expectedIndices);
        CueModel model;
        model.setSearchLanguage(CueModel::Turkish);
        model.replace(
            {{0, 1000, "i", {}}, {1000, 2000, "İ", {}}, {2000, 3000, "ı", {}}, {3000, 4000, "I", {}}});
        model.search(query);
        QList<int> actualIndices;
        for (int row = 0; row < model.count(); ++row) {
            actualIndices.append(model.data(model.index(row), CueModel::CueIndex).toInt());
        }
        QCOMPARE(actualIndices, expectedIndices);
    }
    void languageChangesRefilterAndReplacementClearsSearch() {
        CueModel model;
        model.replace({{0, 1000, "First", {}}, {1000, 2000, "FIRST", {}}});
        model.search("first");
        QCOMPARE(model.count(), 2);
        model.setSearchLanguage(CueModel::Turkish);
        QCOMPARE(model.count(), 1);
        model.setSearchLanguage(CueModel::Unicode);
        QCOMPARE(model.count(), 2);
        model.replace({{0, 1000, "New cue", {}}});
        QCOMPARE(model.count(), 1);
        model.search("first");
        QCOMPARE(model.count(), 0);
        model.search("");
        QCOMPARE(model.count(), 1);
    }
    void multilingualSearchPreservesText_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("query");
        QTest::newRow("Spanish") << QString("junto al río") << QString("RÍO");
        QTest::newRow("Japanese") << QString("日本語の字幕") << QString("日本語");
        QTest::newRow("Arabic") << QString("مرحبا بالعالم") << QString("مرحبا");
        QTest::newRow("decomposed accent") << QString("cafe\u0301") << QString("CAFÉ");
        QTest::newRow("decomposed query") << QString("café") << QString("CAFE\u0301");
        QTest::newRow("decomposed Turkish") << QString("I\u0307zmir") << QString("izmir");
    }
    void multilingualSearchPreservesText() {
        QFETCH(QString, text);
        QFETCH(QString, query);
        CueModel model;
        model.setSearchLanguage(CueModel::Turkish);
        model.replace({{0, 1000, text, {}}});
        model.search(query);
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.data(model.index(0), CueModel::Text).toString(), text);
    }
    void preservesTurkishLetters() {
        const QString text = "İstanbul, İzmir; ışık ve ılık çay.\nÇığ, göl, şüphe — I İ ı i.";
        const auto cues = parseSrt("1\n00:00:21,000 --> 00:00:24,000\n" + text);
        QCOMPARE(cues[0].text, text);
        QCOMPARE(cueAt(cues, 21000), 0);
    }
    void preservesUnicodeAndLiteralMarkup() {
        const auto cues = parseSrt("1\r\n00:00:01,000 --> 00:00:03,000\r\n日本語 <script> & café\r\n");
        QCOMPARE(cues[0].text, QString("日本語 <script> & café"));
        QCOMPARE(cueAt(cues, 999), -1);
        QCOMPARE(cueAt(cues, 1000), 0);
        QCOMPARE(cueAt(cues, 3000), -1);
    }
    void recognizedFormattingIsPlainTextAndUnknownMarkupStaysInert() {
        const auto cues = parseSrt("1\n00:00:01,000 --> 00:00:03,000\nOo <i>yeah</i>! "
                                   "<B>İstanbul</B><br/>ışık &amp; çay <font color=\"red\">şüphe</font>");
        QCOMPARE(cues.first().text, QString("Oo yeah! İstanbul\nışık & çay şüphe"));
        QCOMPARE(subtitlePlainText("<img src=\"https://evil.test/a\"> <script>bad</script>"),
                 QString("<img src=\"https://evil.test/a\"> <script>bad</script>"));
        QCOMPARE(subtitlePlainText("日本語 <i>字幕</i> — <u>مرحبا</u> — río"),
                 QString("日本語 字幕 — مرحبا — río"));
    }
    void transcriptTranslationsUseGreatestOverlapAndPreserveFilteredIdentity() {
        CueModel model;
        model.replace({{1000, 5000, "İstanbul ışığı", {}}, {6000, 8000, "yağmur", {}}});
        model.search("yağmur");
        model.setTranslationCues(
            {{1000, 2000, "Istanbul", {}}, {2000, 4500, "light", {}}, {6500, 7500, "rain", {}}});
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.rowForCue(1), 0);
        QCOMPARE(model.rowForCue(0), -1);
        QCOMPARE(model.data(model.index(0), CueModel::Translation).toString(), QString("rain"));
        model.search("");
        QCOMPARE(model.data(model.index(0), CueModel::Translation).toString(), QString("light"));
    }
    void rejectsMalformedTimings() {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt("1\n00:61:00,000 --> 00:62:00,000\nInvalid"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt("1\n00:00:03,000 --> 00:00:01,000\nInvalid"));
    }
    void sortsAndFiltersWithoutLosingCueIdentity() {
        CueModel model;
        model.replace(
            parseSrt("2\n00:00:04,000 --> 00:00:05,000\nSecond\n\n1\n00:00:01,000 --> 00:00:03,000\nFirst"));
        model.search("SECOND");
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.data(model.index(0), CueModel::CueIndex).toInt(), 1);
        QCOMPARE(cueAt(model.entries(), 3500), -1);
    }
};
QTEST_GUILESS_MAIN(CueTests)
#include "cue_tests.moc"
