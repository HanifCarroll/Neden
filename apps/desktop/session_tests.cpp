#include "learning_session.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

class FakePlayback : public PlaybackEngine {
public:
    FakePlayback() { durationSeconds = 24; }
    double lastSeek = -1;
    bool completeImmediately = true;
    bool applySeekImmediately = true;
    void advance(double value) { positionSeconds = value; emit positionChanged(); }
    void open(const QString &path, bool) override { advance(0); if (completeImmediately) emit loaded(path); }
    void seek(double value) override { lastSeek = value; if (applySeekImmediately) advance(value); }
    void setPaused(bool value) override { isPaused = value; emit pausedChanged(); }
    void rate(double) override {}
    void volume(double) override {}
};

class SessionTests : public QObject {
    Q_OBJECT
private slots:
    void dualSubtitlesAreSessionOnlyWhileVideoPositionsSurviveRestart() {
        QTemporaryDir directory;
        const auto videoPath = directory.filePath("Leyla.webm");
        const auto otherPath = directory.filePath("Other.webm");
        for (const auto &path : {videoPath, otherPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto source = directory.filePath("source.tr.srt");
        const auto translation = directory.filePath("translation.en.srt");
        QFile sourceFile(source);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        sourceFile.write(QString("1\n00:00:00,000 --> 00:00:24,000\nBir ışık gördüm.\n").toUtf8());
        sourceFile.close();
        QFile translationFile(translation);
        QVERIFY(translationFile.open(QIODevice::WriteOnly));
        translationFile.write("1\n00:00:00,000 --> 00:00:24,000\nI saw a light.\n");
        translationFile.close();

        // 1. Closing and switching videos preserve independent resume positions.
        {
            RecentVideos recent(directory.path());
            FakePlayback playback;
            LearningSession session(&playback);
            session.setRecentVideos(&recent);
            session.openVideo(QUrl::fromLocalFile(videoPath));
            session.openSubtitles(QUrl::fromLocalFile(source));
            session.openTranslation(QUrl::fromLocalFile(translation));
            playback.advance(12.345678);
            QCOMPARE(session.subtitle(), QString("Bir ışık gördüm."));
            QCOMPARE(session.translation(), QString("I saw a light."));
            session.openVideo(QUrl::fromLocalFile(otherPath));
            playback.advance(6.125);
        }

        // 2. A new app session restores position without retaining subtitle choices.
        RecentVideos recent(directory.path());
        FakePlayback playback;
        LearningSession session(&playback);
        session.setRecentVideos(&recent);
        session.openVideo(QUrl::fromLocalFile(videoPath));
        QCOMPARE(playback.lastSeek, 12.345678);
        QVERIFY(session.subtitle().isEmpty());
        QVERIFY(session.translation().isEmpty());
        QVERIFY(!playback.paused());
        session.openVideo(QUrl::fromLocalFile(otherPath));
        QCOMPARE(playback.lastSeek, 6.125);
        QVERIFY(session.subtitle().isEmpty());
        QVERIFY(session.translation().isEmpty());
    }
    void finderUnicodePathResumesTheExistingMacVideo() {
#ifndef Q_OS_MACOS
        QSKIP("This regression concerns the Mac filesystem's Unicode-equivalent paths.");
#else
        QTemporaryDir directory;
        const QString composed = directory.filePath(QString::fromUtf8("Bölüm.webm"));
        const QString decomposed = composed.normalized(QString::NormalizationForm_D);
        QFile file(composed);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(QFile::exists(decomposed));
        RecentVideos recent(directory.path());
        QVERIFY(recent.remember({composed, 12.345, 24}, true));
        FakePlayback playback;
        LearningSession session(&playback);
        session.setRecentVideos(&recent);
        session.openVideo(QUrl::fromLocalFile(decomposed));
        QCOMPARE(playback.lastSeek, 12.345);
        QCOMPARE(recent.count(), 1);
        QCOMPARE(recent.find(decomposed)->position, 12.345);
        recent.remove(QUrl::fromLocalFile(decomposed));
        session.checkpoint();
        QCOMPARE(recent.count(), 0);
#endif
    }
    void removingCurrentVideoDoesNotReinsertItOnClose() {
        QTemporaryDir directory;
        QFile file(directory.filePath("video.webm"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        RecentVideos recent(directory.path());
        {
            FakePlayback playback;
            LearningSession session(&playback);
            session.setRecentVideos(&recent);
            session.openVideo(QUrl::fromLocalFile(file.fileName()));
            recent.remove(QUrl::fromLocalFile(file.fileName()));
            playback.advance(10);
            session.checkpoint();
        }
        QCOMPARE(recent.count(), 0);
        QVERIFY(file.exists());
    }
    void lateLoadCannotApplyAnotherVideosResumeState() {
        QTemporaryDir directory;
        const auto first = directory.filePath("first.webm");
        const auto second = directory.filePath("second.webm");
        for (const auto &path : {first, second}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        RecentVideos recent(directory.path());
        QVERIFY(recent.remember({first, 12.5, 24}, true));
        FakePlayback playback;
        playback.completeImmediately = false;
        LearningSession session(&playback);
        session.setRecentVideos(&recent);
        session.openVideo(QUrl::fromLocalFile(first));
        session.openVideo(QUrl::fromLocalFile(second));
        emit playback.loaded(first);
        QCOMPARE(playback.lastSeek, -1.0);
        emit playback.loaded(second);
        QCOMPARE(playback.lastSeek, -1.0);
        QCOMPARE(recent.find(first)->position, 12.5);
    }
    void closeDuringResumeDoesNotReplaceSavedPositionWithZero() {
        QTemporaryDir directory;
        QFile file(directory.filePath("video.webm"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        RecentVideos recent(directory.path());
        QVERIFY(recent.remember({file.fileName(), 12.345, 24}, true));
        FakePlayback playback;
        playback.applySeekImmediately = false;
        LearningSession session(&playback);
        session.setRecentVideos(&recent);
        session.openVideo(QUrl::fromLocalFile(file.fileName()));
        session.checkpoint();
        QCOMPARE(recent.find(file.fileName())->position, 12.345);
        playback.advance(12.378);
        session.checkpoint();
        QCOMPARE(recent.find(file.fileName())->position, 12.378);
    }
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
        QFile video(directory.filePath("next.mp4"));
        QVERIFY(video.open(QIODevice::WriteOnly));
        video.close();
        session.openVideo(QUrl::fromLocalFile(video.fileName()));
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
