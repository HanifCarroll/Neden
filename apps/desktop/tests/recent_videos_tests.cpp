#include "recent_videos.h"
#include <QFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtTest>

class RecentVideoTests : public QObject {
    Q_OBJECT
private slots:
    void persistsIndependentPositionsAndSubtitleFilesWithoutLanguagePresets() {
        QTemporaryDir directory;
        const auto first = directory.filePath("İstanbul.webm");
        const auto second = directory.filePath("Second.webm");
        const auto primary = directory.filePath("source.tr.srt");
        const auto secondary = directory.filePath("translation.en.srt");
        {
            RecentVideos videos(directory.path());
            QVERIFY(videos.remember({first, 1234.56789, 4300, primary, secondary}, true));
            QVERIFY(videos.remember({second, 18.125, 900}, true));
        }
        RecentVideos restored(directory.path());
        QCOMPARE(restored.count(), 2);
        QCOMPARE(restored.data(restored.index(0), RecentVideos::MediaTitle).toString(), QString("Second"));
        QCOMPARE(restored.find(first)->position, 1234.56789);
        QCOMPARE(restored.find(first)->primarySubtitlePath, primary);
        QCOMPARE(restored.find(first)->secondarySubtitlePath, secondary);
        QVERIFY(restored.find(second)->primarySubtitlePath.isEmpty());
        QFile savedFile(directory.filePath("recent-videos.json"));
        QVERIFY(savedFile.open(QIODevice::ReadOnly));
        const auto savedBytes = savedFile.readAll();
        QVERIFY(!savedBytes.contains("language"));
        QVERIFY(restored.remember({first, 1250.5, 4300}, true));
        QCOMPARE(restored.count(), 2);
        QCOMPARE(restored.data(restored.index(0), RecentVideos::MediaTitle).toString(), QString("İstanbul"));
    }
    void olderHistoryLoadsWithoutLosingPositions() {
        QTemporaryDir directory;
        const auto video = directory.filePath("video.webm");
        const QJsonObject oldEntry{{"path", video}, {"position", 12.345678}, {"duration", 100}};
        QFile file(directory.filePath("recent-videos.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{"version", 1}, {"videos", QJsonArray{oldEntry}}}).toJson());
        file.close();
        RecentVideos recent(directory.path());
        QVERIFY(recent.error().isEmpty());
        QCOMPARE(recent.find(video)->position, 12.345678);
        QVERIFY(recent.find(video)->primarySubtitlePath.isEmpty());
        QVERIFY(recent.remember({video, 20, 100}, true));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value("version").toInt(), 4);
    }
    void completionFiltersWithoutLosingSettingsAndSurvivesCheckpoints() {
        QTemporaryDir directory;
        const auto path = directory.filePath("episode.webm");
        RecentVideos videos(directory.path());
        RecentVideosFilter unfinished;
        unfinished.setHistory(&videos);
        RecentVideo video{path, 95, 100, directory.filePath("primary.srt"), directory.filePath("secondary.srt")};
        video.primaryDelay = 0.5;
        video.secondaryNativeTrack = "3";
        QVERIFY(videos.remember(video, true));
        QCOMPARE(unfinished.rowCount(), 1); // Near the end is not completion.
        videos.setFinished(QUrl::fromLocalFile(path), true);
        QCOMPARE(unfinished.rowCount(), 0);
        QCOMPARE(videos.count(), 1);
        video.position = 98;
        QVERIFY(videos.remember(video));
        QVERIFY(videos.find(path)->finished);
        QCOMPARE(unfinished.rowCount(), 0);
        RecentVideos restored(directory.path());
        QVERIFY(restored.find(path)->finished);
        QCOMPARE(restored.find(path)->primarySubtitlePath, video.primarySubtitlePath);
        QCOMPARE(restored.find(path)->secondarySubtitlePath, video.secondarySubtitlePath);
        QCOMPARE(restored.find(path)->primaryDelay, 0.5);
        QCOMPARE(restored.find(path)->secondaryNativeTrack, QString("3"));
        unfinished.setIncludeFinished(true);
        QCOMPARE(unfinished.rowCount(), 1);
        QVERIFY(unfinished.data(unfinished.index(0, 0), RecentVideos::Finished).toBool());
        unfinished.setIncludeFinished(false);
        videos.setFinished(QUrl::fromLocalFile(path), false);
        QCOMPARE(unfinished.rowCount(), 1);
        videos.setFinished(QUrl::fromLocalFile(path), true);
        video.position = 0;
        QVERIFY(videos.remember(video, true));
        QVERIFY(!videos.find(path)->finished);
        QCOMPARE(unfinished.rowCount(), 1);
    }
    void removalPersistsWithoutDeletingMedia() {
        QTemporaryDir directory;
        QFile media(directory.filePath("video.webm"));
        QVERIFY(media.open(QIODevice::WriteOnly));
        media.close();
        RecentVideos videos(directory.path());
        QVERIFY(videos.remember({media.fileName(), 42.125, 100}, true));
        videos.remove(QUrl::fromLocalFile(media.fileName()));
        QCOMPARE(videos.count(), 0);
        QVERIFY(media.exists());
        RecentVideos restored(directory.path());
        QCOMPARE(restored.count(), 0);
    }
    void malformedHistoryIsPreserved() {
        QTemporaryDir directory;
        QFile file(directory.filePath("recent-videos.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{broken history");
        file.close();
        RecentVideos videos(directory.path());
        QVERIFY(!videos.error().isEmpty());
        QVERIFY(!videos.remember({directory.filePath("new.webm"), 1, 2}, true));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{broken history"));
    }
    void rejectsRemotePathsAndNonfinitePositions() {
        QTemporaryDir directory;
        RecentVideos videos(directory.path());
        QVERIFY(!videos.remember({"https://example.invalid/video.mp4", 1, 2}));
        QVERIFY(!videos.remember({directory.filePath("video.mp4"), qQNaN(), 2}));
        QVERIFY(!videos.remember(
            {directory.filePath("video.mp4"), 1, 2, "https://example.org/subtitles.srt", {}}));
        QCOMPARE(videos.count(), 0);
    }
};
QTEST_GUILESS_MAIN(RecentVideoTests)
#include "recent_videos_tests.moc"
