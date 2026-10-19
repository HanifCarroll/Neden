#include "learning_session.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

class FakePlayback : public PlaybackEngine {
public:
    FakePlayback() { durationSeconds = 24; }
    double lastSeek = -1;
    void advance(double value) { positionSeconds = value; emit positionChanged(); }
    void open(const QString &, bool) override { advance(0); emit loaded(); }
    void seek(double value) override { lastSeek = value; advance(value); }
    void setPaused(bool value) override { isPaused = value; emit pausedChanged(); }
    void rate(double) override {}
    void volume(double) override {}
};

class SessionTests : public QObject {
    Q_OBJECT
private slots:
    void independentTranslationTimingAndMediaReset() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{0, 6000, "Bir ışık gördüm.", "fixture"}});
        QTemporaryDir directory;
        QFile file(directory.filePath("translation.srt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("1\n00:00:01,000 --> 00:00:02,000\nI saw a light.\n");
        file.close();
        session.openTranslation(QUrl::fromLocalFile(file.fileName()));
        QVERIFY(session.translation().isEmpty());
        engine.advance(1.5);
        QCOMPARE(session.translation(), QString("I saw a light."));
        engine.advance(2.5);
        QVERIFY(session.translation().isEmpty());
        session.openVideo(QUrl::fromLocalFile(directory.filePath("next.mp4")));
        QVERIFY(session.translation().isEmpty());
    }
    void localSubtitleDecodingRejectsInvalidUtf8WithoutReplacingTrack() {
        FakePlayback engine;
        LearningSession session(&engine);
        QTemporaryDir directory;
        QFile file(directory.filePath("source.srt"));

        // 1. Load a substantial original Unicode fixture through the real file decoder.
        const QString phrase = QString("İstanbul ışığı 日本語 ").repeated(120);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(("1\n00:00:00,000 --> 00:00:03,000\n" + phrase + "\n").toUtf8());
        file.close();
        session.openSubtitles(QUrl::fromLocalFile(file.fileName()));
        QCOMPARE(session.subtitle(), phrase.trimmed());

        // 2. Invalid bytes must fail before replacing the successfully loaded track.
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QByteArray("1\n00:00:00,000 --> 00:00:03,000\n") + char(0xff));
        file.close();
        session.openSubtitles(QUrl::fromLocalFile(file.fileName()));
        QCOMPARE(session.subtitle(), phrase.trimmed());
    }
    void clockOnlyNotifiesCueWhenItChanges() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{0, 3000, "İstanbul", "Istanbul"}, {3000, 6000, "ışık", "light"}});
        QSignalSpy changes(&session, &LearningSession::cueChanged);
        engine.advance(0.5);
        engine.advance(1.5);
        QCOMPARE(changes.count(), 0);
        engine.advance(3.5);
        QCOMPARE(changes.count(), 1);
        QCOMPARE(session.subtitle(), QString("ışık"));
    }
    void selectionRestoresOnlyPriorPlayingState() {
        FakePlayback engine;
        LearningSession session(&engine);
        engine.setPaused(false);
        session.select("İstanbul");
        QVERIFY(engine.paused());
        session.closeDefinition();
        QVERIFY(!engine.paused());
        engine.setPaused(true);
        session.select("İstanbul");
        session.closeDefinition();
        QVERIFY(engine.paused());
    }
    void replayRestoresPositionAndPause() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{0, 3000, "İstanbul", "Istanbul"}});
        engine.advance(1.2);
        session.replay();
        QCOMPARE(engine.lastSeek, 0);
        QVERIFY(!engine.paused());
        session.stopReplay();
        QCOMPARE(engine.lastSeek, 1.2);
        QVERIFY(engine.paused());
        QVERIFY(!session.replaying());
    }
    void seekCancelsSelectionWithoutResuming() {
        FakePlayback engine;
        LearningSession session(&engine);
        engine.setPaused(false);
        session.select("İstanbul");
        session.seek(3.5);
        session.closeDefinition();
        QVERIFY(session.selection().isEmpty());
        QVERIFY(engine.paused());
    }
};
QTEST_GUILESS_MAIN(SessionTests)
#include "session_tests.moc"
