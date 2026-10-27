#include "embedded_subtitles.h"
#include <QTemporaryFile>
#include <QtTest>
class EmbeddedSubtitleTests : public QObject {
    Q_OBJECT
private slots:
    void readsTextFormats_data() {
        QTest::addColumn<QString>("file");
        QTest::newRow("SRT in Matroska") << "text.mkv";
        QTest::newRow("ASS in Matroska") << "styled.mkv";
        QTest::newRow("mov_text in MP4") << "text.mp4";
    }
    void readsTextFormats() {
        QFETCH(QString, file);
        const QString path = QFINDTESTDATA("fixtures/subtitles/" + file);
        std::atomic_bool cancelled{false};
        const auto cues = EmbeddedSubtitles::read(path, 1, cancelled);
        QCOMPARE(cues.size(), 2);
        QCOMPARE(cues[0].startMs, 500LL);
        QCOMPARE(cues[0].endMs, 1750LL);
        QCOMPARE(cues[0].text, QString("Hello, world."));
        QCOMPARE(cues[1].startMs, 2250LL);
        QCOMPARE(cues[1].endMs, 4500LL);
        QCOMPARE(cues[1].text, QString("İyi akşamlar.\nTwo lines."));
    }
    void rejectsWrongStreamsAndCancelledOrDamagedFiles() {
        const auto path = QFINDTESTDATA("fixtures/subtitles/text.mkv");
        std::atomic_bool cancelled{false};
        QVERIFY_EXCEPTION_THROWN(EmbeddedSubtitles::read(path, 0, cancelled), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(EmbeddedSubtitles::read(path, 100, cancelled), std::runtime_error);
        cancelled = true;
        QVERIFY_EXCEPTION_THROWN(EmbeddedSubtitles::read(path, 1, cancelled), std::runtime_error);
        cancelled = false;
        QTemporaryFile invalid;
        QVERIFY(invalid.open());
        invalid.write("not a media file");
        invalid.flush();
        QVERIFY_EXCEPTION_THROWN(EmbeddedSubtitles::read(invalid.fileName(), 0, cancelled),
                                 std::runtime_error);
    }
};
QTEST_GUILESS_MAIN(EmbeddedSubtitleTests)
#include "embedded_subtitle_tests.moc"
