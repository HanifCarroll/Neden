#include "mpv_playback.h"
#include "screenshot_image.h"
#include <QtTest>

class MpvPlaybackTests : public QObject {
    Q_OBJECT
private slots:
    void rawScreenshotCopiesPaddedBgrPixelsAndRejectsTruncatedBuffers() {
        unsigned char pixels[]{0, 0, 255, 0, 9, 9, 9, 9, 255, 0, 0, 0};
        mpv_byte_array bytes{pixels, sizeof(pixels)};
        char widthKey[] = "w", heightKey[] = "h", strideKey[] = "stride";
        char formatKey[] = "format", dataKey[] = "data", format[] = "bgr0";
        char *keys[]{widthKey, heightKey, strideKey, formatKey, dataKey};
        mpv_node values[5]{};
        for (int index = 0; index < 3; ++index)
            values[index].format = MPV_FORMAT_INT64;
        values[0].u.int64 = 1;
        values[1].u.int64 = 2;
        values[2].u.int64 = 8;
        values[3].format = MPV_FORMAT_STRING;
        values[3].u.string = format;
        values[4].format = MPV_FORMAT_BYTE_ARRAY;
        values[4].u.ba = &bytes;
        mpv_node_list fields{5, values, keys};
        mpv_node result{};
        result.format = MPV_FORMAT_NODE_MAP;
        result.u.list = &fields;
        const auto image = screenshotImage(result);
        QCOMPARE(image.size(), QSize(1, 2));
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(image.pixelColor(0, 1), QColor(Qt::blue));
        pixels[2] = 0;
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
        bytes.size = 11;
        QVERIFY(screenshotImage(result).isNull());
        bytes.size = sizeof(pixels);
        values[2].u.int64 = -8;
        QVERIFY(screenshotImage(result).isNull());
    }
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
