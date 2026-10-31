#include "cues.h"
#include "subtitle_document.h"
#include "property_support.h"
#include <algorithm>
#include <limits>

class SubtitlePropertyTests : public QObject {
    Q_OBJECT
    static QVector<Cue> track(std::mt19937 &random, bool overlapping) {
        QVector<Cue> cues;
        qint64 start = 10000;
        const int count = PropertySupport::number(random, 1, 80);
        for (int index = 0; index < count; ++index) {
            // At least 2 ms so every allowed rate leaves a representable positive interval.
            const int duration = PropertySupport::number(random, 2, 10000);
            cues.append({start, start + duration, PropertySupport::text(random), "translation"});
            start += overlapping ? PropertySupport::number(random, 0, 2000) : duration + 1;
        }
        return cues;
    }
private slots:
    void literalMarkupRegression() {
        const QVector<Cue> original{{0, 1, "<i>x</i> &amp;", {}}};
        QCOMPARE(parseSrt(QString::fromUtf8(SubtitleDocument::serialize(original))).first().text,
                 original.first().text);
    }
    void timestampOverflowRegression() {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt("3000000000000:00:00,000 --> 00:01,000\nn"));
        QVERIFY_THROWS_EXCEPTION(
            std::runtime_error,
            parseSrt("1\n99999999999999999999:00:00,000 --> 99999999999999999999:00:01,000\nx"));
        const auto maximum = parseSrt("2562047788015:12:55,806 --> 2562047788015:12:55,807\nx");
        QCOMPARE(maximum.first().endMs, std::numeric_limits<qint64>::max());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 parseSrt("2562047788015:12:55,807 --> 2562047788015:12:55,808\nx"));
    }
    void parserLimitsAreEnforced() {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt(QString(8 * 1024 * 1024 + 1, 'x')));
        const QString timing = "1\n00:00:00,000 --> 00:00:01,000\n";
        QCOMPARE(parseSrt(timing + QString(4000, 'x')).first().text.size(), 4000);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt(timing + QString(4001, 'x')));
        const auto cue = timing + "x\n\n";
        QCOMPARE(parseSrt(cue.repeated(50000)).size(), 50000);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt(cue.repeated(50001)));
    }
    void roundTripsPreservePlainTextAndTimings_data() { PropertySupport::seeds(); }
    void roundTripsPreservePlainTextAndTimings() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        auto source = track(random, true);
        for (int index = source.size() - 1; index > 0; --index)
            std::swap(source[index], source[PropertySupport::number(random, 0, index)]);
        const auto encoded = SubtitleDocument::serialize(source);
        const auto parsed = parseSrt(QString::fromUtf8(encoded));
        std::stable_sort(source.begin(), source.end(),
                         [](const Cue &left, const Cue &right) { return left.startMs < right.startMs; });
        QCOMPARE(parsed.size(), source.size());
        for (int index = 0; index < parsed.size(); ++index) {
            QCOMPARE(parsed[index].startMs, source[index].startMs);
            QCOMPARE(parsed[index].endMs, source[index].endMs);
            QCOMPARE(parsed[index].text, source[index].text);
        }
        // CRLF and a file BOM have the same meaning as LF.
        const auto crlf = parseSrt(QChar(0xfeff) + QString::fromUtf8(encoded).replace("\n", "\r\n"));
        QCOMPARE(SubtitleDocument::serialize(crlf), SubtitleDocument::serialize(parsed));
    }

    void retimingPreservesDialogueAndValidIntervals_data() { PropertySupport::seeds(); }
    void retimingPreservesDialogueAndValidIntervals() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        const auto source = track(random, true);
        const qint64 offset = PropertySupport::number(random, -5000, 5000);
        const auto shifted = SubtitleDocument::retime(source, {1, double(offset), 1});
        const auto restored = SubtitleDocument::retime(shifted, {1, -double(offset), 1});
        const double rate = PropertySupport::number(random, 80, 125) / 100.0;
        const auto scaled = SubtitleDocument::retime(source, {rate, 0, 1});
        QCOMPARE(shifted.size(), source.size());
        for (int index = 0; index < source.size(); ++index) {
            QCOMPARE(shifted[index].text, source[index].text);
            QCOMPARE(shifted[index].startMs, source[index].startMs + offset);
            QCOMPARE(shifted[index].endMs - shifted[index].startMs,
                     source[index].endMs - source[index].startMs);
            QCOMPARE(restored[index].startMs, source[index].startMs);
            QCOMPARE(restored[index].endMs, source[index].endMs);
            QCOMPARE(scaled[index].text, source[index].text);
            QVERIFY(scaled[index].startMs >= 0);
            QVERIFY(scaled[index].endMs > scaled[index].startMs);
            QVERIFY(std::abs(scaled[index].startMs - source[index].startMs * rate) <= 0.500001);
        }
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, SubtitleDocument::retime(source, {1, -86400000, 1}));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, SubtitleDocument::retime(source, {NAN, 0, 1}));
    }

    void lookupMatchesLinearOracleAtBoundaries_data() { PropertySupport::seeds(); }
    void lookupMatchesLinearOracleAtBoundaries() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        const auto cues = track(random, false);
        for (const auto &cue : cues) {
            for (qint64 time : {cue.startMs - 1, cue.startMs, cue.endMs - 1, cue.endMs}) {
                int expected = -1;
                for (int index = 0; index < cues.size(); ++index)
                    if (cues[index].startMs <= time && time < cues[index].endMs)
                        expected = index;
                QCOMPARE(cueAt(cues, time), expected);
            }
        }
    }

    void malformedHoursAreRejected_data() { PropertySupport::seeds(); }
    void malformedHoursAreRejected() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        const QString hours(PropertySupport::number(random, 20, 200), QChar('9'));
        const QString input = "1\n" + hours + ":00:00,000 --> " + hours + ":00:01,000\nText";
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, parseSrt(input));
    }
};
QTEST_GUILESS_MAIN(SubtitlePropertyTests)
#include "subtitle_property_tests.moc"
