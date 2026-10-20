#include "mpv_playback.h"
#include <QtTest>

class MpvPlaybackTests : public QObject {
    Q_OBJECT
private slots:
    void playbackOptionsAreSupportedByTheBundledRuntime() {
        MpvPlayback playback;
        QSignalSpy errors(&playback, &PlaybackEngine::operationFailed);
        playback.configure(PlaybackEngine::Option::PrimarySubtitleDelay, 0.4);
        playback.configure(PlaybackEngine::Option::SecondarySubtitleDelay, -0.7);
        playback.configure(PlaybackEngine::Option::AudioDelay, 0.2);
        playback.configure(PlaybackEngine::Option::Rotation, 90);
        playback.configure(PlaybackEngine::Option::AspectRatio, "4:3");
        playback.configure(PlaybackEngine::Option::Fill, true);
        playback.configure(PlaybackEngine::Option::SubtitleEncoding, "windows-1254");
        playback.configure(PlaybackEngine::Option::Deinterlace, true);
        QCOMPARE(errors.count(), 0);
        double secondaryDelay = 0;
        const int result =
            mpv_get_property(playback.handle(), "secondary-sub-delay", MPV_FORMAT_DOUBLE, &secondaryDelay);
        QCOMPARE(playback.supportsIndependentNativeSubtitleTiming(), result >= 0);
        if (result >= 0)
            QCOMPARE(secondaryDelay, -0.7);
        int correctPitch = 0;
        QCOMPARE(
            mpv_get_property(playback.handle(), "audio-pitch-correction", MPV_FORMAT_FLAG, &correctPitch), 0);
        QCOMPARE(correctPitch, 1);
    }
    void idleControlsDoNotRequestVideoOutput() {
        MpvPlayback playback;
        playback.volume(35);
        playback.rate(1.25);
        playback.setPaused(false);
        playback.seek(12);
        QVERIFY(!playback.videoOutputRequested());
        QVERIFY(playback.sourceOutputSize().isEmpty());
        QVERIFY(!playback.paused());
    }

    void openingWaitsForRendererAndPreservesIdlePreferences() {
        MpvPlayback playback;
        playback.volume(35);
        playback.rate(1.25);
        QSignalSpy outputNeeded(&playback, &MpvPlayback::videoOutputNeeded);
        QSignalSpy loaded(&playback, &PlaybackEngine::loaded);
        QSignalSpy failed(&playback, &PlaybackEngine::failed);

        playback.open("/missing/first.mp4", false);
        playback.open("/missing/latest.mp4", true);
        QVERIFY(playback.handle());
        QCOMPARE(outputNeeded.count(), 2);
        QVERIFY(playback.videoOutputRequested());
        QVERIFY(playback.paused());

        double speed = 0;
        double volume = 0;
        QCOMPARE(mpv_get_property(playback.handle(), "speed", MPV_FORMAT_DOUBLE, &speed), 0);
        QCOMPARE(mpv_get_property(playback.handle(), "volume", MPV_FORMAT_DOUBLE, &volume), 0);
        QCOMPARE(speed, 1.25);
        QCOMPARE(volume, 35.0);
        QTest::qWait(100);
        QCOMPARE(loaded.count(), 0);
        QCOMPARE(failed.count(), 0);
    }
};

QTEST_GUILESS_MAIN(MpvPlaybackTests)
#include "mpv_playback_tests.moc"
