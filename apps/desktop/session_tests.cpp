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
    double audioLevel = -1;
    void volume(double value) override { audioLevel = value; }
};

class SessionTests : public QObject {
    Q_OBJECT
private slots:
    void loadedTracksCanBeReusedOrDisabledIndependently() {
        QTemporaryDir directory;
        const auto first = directory.filePath("source.vtt");
        const auto second = directory.filePath("secondary.srt");
        for (const auto &path : {first, second}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("1\n00:00:00,000 --> 00:00:24,000\nSubtitle\n");
        }
        FakePlayback playback;
        LearningSession session(&playback);
        session.openSubtitles(QUrl::fromLocalFile(first));
        session.openTranslation(QUrl::fromLocalFile(second));
        QCOMPARE(session.subtitleTracks().size(), 2);
        session.clearTrack(true);
        QVERIFY(session.translation().isEmpty());
        QVERIFY(!session.subtitle().isEmpty());
        QVERIFY(session.model.cues.first().translation.isEmpty());
        session.chooseTrack(first, true);
        QCOMPARE(session.translationSubtitleName(), QString("source.vtt"));
        session.clearTrack(false);
        QVERIFY(session.subtitle().isEmpty());
        QVERIFY(!session.translation().isEmpty());
        QCOMPARE(session.subtitleTracks().size(), 2);
    }

    void volumeIsBoundedAndUnmuteRestoresTheSliderLevel() {
        FakePlayback playback;
        LearningSession session(&playback);
        session.volume(37);
        QCOMPARE(session.volumeLevel(), 37);
        QCOMPARE(playback.audioLevel, 37);
        session.toggleMute();
        QCOMPARE(session.volumeLevel(), 0);
        session.toggleMute();
        QCOMPARE(session.volumeLevel(), 37);
        session.volume(150);
        QCOMPARE(session.volumeLevel(), 100);
        session.volume(-5);
        QCOMPARE(session.volumeLevel(), 0);
        session.volume(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(session.volumeLevel(), 0);
    }
    void dualSubtitleRolesAndVideoPositionsSurviveRestart() {
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
            session.openSubtitles(QUrl::fromLocalFile(translation));
            session.openTranslation(QUrl::fromLocalFile(source));
            playback.advance(6.125);
        }

        // 2. Each video restores its own chosen tracks and roles, without language presets.
        RecentVideos recent(directory.path());
        FakePlayback playback;
        LearningSession session(&playback);
        session.setRecentVideos(&recent);
        session.openVideo(QUrl::fromLocalFile(videoPath));
        QCOMPARE(playback.lastSeek, 12.345678);
        QCOMPARE(session.subtitle(), QString("Bir ışık gördüm."));
        QCOMPARE(session.translation(), QString("I saw a light."));
        QCOMPARE(session.sourceSubtitleName(), QString("source.tr.srt"));
        QCOMPARE(session.translationSubtitleName(), QString("translation.en.srt"));
        QVERIFY(!playback.paused());
        session.openVideo(QUrl::fromLocalFile(otherPath));
        QCOMPARE(playback.lastSeek, 6.125);
        QCOMPARE(session.subtitle(), QString("I saw a light."));
        QCOMPARE(session.translation(), QString("Bir ışık gördüm."));
    }
    void missingTrackIsRetainedWhileTheOtherTrackAndPositionRestore() {
        QTemporaryDir directory;
        const auto video = directory.filePath("video.webm");
        const auto primary = directory.filePath("source.srt");
        const auto secondary = directory.filePath("translation.srt");
        const auto replacement = directory.filePath("replacement.srt");
        for (const auto &path : {video, secondary, replacement}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("1\n00:00:00,000 --> 00:00:24,000\nI saw a light.\n");
        }
        RecentVideos recent(directory.path());
        QVERIFY(recent.remember({video, 12.25, 24, primary, secondary}, true));
        {
            FakePlayback playback;
            LearningSession session(&playback);
            session.setRecentVideos(&recent);
            session.openVideo(QUrl::fromLocalFile(video));
            QCOMPARE(playback.lastSeek, 12.25);
            QVERIFY(!playback.paused());
            QVERIFY(session.subtitle().isEmpty());
            QCOMPARE(session.translation(), QString("I saw a light."));
            QVERIFY(session.status().contains("saved primary"));
            session.checkpoint();
            QCOMPARE(recent.find(video)->primarySubtitlePath, primary);
        }

        // A reconnected file restores without choosing it again.
        QFile primaryFile(primary);
        QVERIFY(primaryFile.open(QIODevice::WriteOnly));
        primaryFile.write(QString("1\n00:00:00,000 --> 00:00:24,000\nBir ışık gördüm.\n").toUtf8());
        primaryFile.close();
        FakePlayback playback;
        LearningSession session(&playback);
        session.setRecentVideos(&recent);
        session.openVideo(QUrl::fromLocalFile(video));
        QCOMPARE(session.subtitle(), QString("Bir ışık gördüm."));

        // Changing a track at an unchanged position must still save its replacement.
        session.openSubtitles(QUrl::fromLocalFile(replacement));
        RecentVideos restored(directory.path());
        QCOMPARE(restored.find(video)->primarySubtitlePath, replacement);
        QCOMPARE(restored.find(video)->secondarySubtitlePath, secondary);
        QCOMPARE(restored.find(video)->position, 12.25);
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
    void subtitleNavigationPlaysOnceAndHandlesGapsAndBoundaries() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{1000, 3000, "İstanbul", {}}, {5000, 7000, "ışık", {}}, {9000, 11000, "yağmur", {}}});
        QVERIFY(!session.canPreviousSubtitle());
        QVERIFY(session.canNextSubtitle());
        session.nextSubtitle();
        QCOMPARE(engine.lastSeek, 1);
        QVERIFY(!engine.paused());
        engine.advance(3);
        QVERIFY(engine.paused());
        QCOMPARE(engine.lastSeek, 2.95);
        session.seek(5.5);
        session.previousSubtitle();
        QCOMPARE(engine.lastSeek, 1);
        QVERIFY(!engine.paused());
        session.nextSubtitle();
        QCOMPARE(engine.lastSeek, 5);
        QVERIFY(!engine.paused());
        engine.advance(7);
        QVERIFY(engine.paused());
        QCOMPARE(engine.lastSeek, 6.95);
        session.seek(4);
        session.previousSubtitle();
        QCOMPARE(engine.lastSeek, 1);
        session.seek(4);
        session.nextSubtitle();
        QCOMPARE(engine.lastSeek, 5);
        session.seek(9.5);
        QVERIFY(!session.canNextSubtitle());
        session.nextSubtitle();
        QCOMPARE(engine.lastSeek, 9.5);
        session.seek(12);
        session.previousSubtitle();
        QCOMPARE(engine.lastSeek, 9);
    }
    void repeatedSubtitleStepsAdvanceWhileTheDecoderIsStillSeeking() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{1000, 3000, "İstanbul", {}}, {5000, 7000, "ışık", {}}, {9000, 11000, "yağmur", {}}});
        engine.advance(1.5);
        engine.applySeekImmediately = false;
        session.nextSubtitle();
        QCOMPARE(engine.lastSeek, 5);
        session.nextSubtitle();
        QCOMPARE(engine.lastSeek, 9);
        QVERIFY(!session.canNextSubtitle());
        engine.advance(5); // A late update from the first seek cannot replace the requested target.
        session.previousSubtitle();
        QCOMPARE(engine.lastSeek, 5);
        session.previousSubtitle();
        QCOMPARE(engine.lastSeek, 1);
        QVERIFY(!engine.paused());
        engine.advance(1);
        engine.advance(3);
        QVERIFY(engine.paused());
        QVERIFY(!session.replaying());
        QCOMPARE(engine.lastSeek, 2.95);
    }
    void replayPlaysTheCueThenPausesAtItsEnd() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{1000, 3000, "İstanbul", {}}, {3000, 5000, "ışık", {}}});
        engine.advance(1.2);
        engine.setPaused(false);
        session.replay();
        QCOMPARE(engine.lastSeek, 1);
        QVERIFY(!engine.paused());
        engine.advance(3.1);
        QVERIFY(engine.paused());
        QVERIFY(!session.replaying());
        QCOMPARE(engine.lastSeek, 2.95);
        QCOMPARE(session.activeCue(), 0);
        QCOMPARE(session.subtitle(), QString("İstanbul"));
        session.replay();
        engine.advance(1.5);
        session.stopReplay();
        QCOMPARE(engine.position(), 1.5);
        QVERIFY(engine.paused());
    }
    void delayedReplaySeekMustArriveBeforeItCanFinish() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{1000, 3000, "İstanbul", {}}, {5000, 7000, "ışık", {}}});
        engine.advance(1.5);
        engine.applySeekImmediately = false;
        session.replay();
        engine.advance(8);
        QVERIFY(session.replaying());
        engine.advance(1);
        engine.advance(3);
        QVERIFY(!session.replaying());
        QVERIFY(engine.paused());
        QCOMPARE(engine.lastSeek, 2.95);
    }
    void navigationReplacesReplayAndDefinitionWithTheChosenCue() {
        FakePlayback engine;
        LearningSession session(&engine);
        session.replaceCues({{1000, 3000, "İstanbul", {}}, {5000, 7000, "ışık", {}}});
        engine.advance(1.5);
        engine.setPaused(false);
        session.select("İstanbul");
        session.nextSubtitle();
        session.closeDefinition();
        QVERIFY(!engine.paused());
        QVERIFY(session.selection().isEmpty());
        QCOMPARE(engine.lastSeek, 5);
        session.replay();
        session.previousSubtitle();
        QCOMPARE(engine.lastSeek, 1);
        QVERIFY(!engine.paused());
        QVERIFY(session.replaying());
        engine.advance(3);
        QVERIFY(engine.paused());
        QVERIFY(!session.replaying());
        session.replaceCues({});
        QVERIFY(!session.canPreviousSubtitle());
        QVERIFY(!session.canNextSubtitle());
        session.previousSubtitle();
        session.nextSubtitle();
        session.replay();
        QVERIFY(engine.paused());
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
