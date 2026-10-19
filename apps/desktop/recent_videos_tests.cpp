#include "recent_videos.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class RecentVideoTests : public QObject {
    Q_OBJECT
private slots:
    void persistsIndependentPositionsWithoutSubtitleChoices() {
        QTemporaryDir directory;
        const auto first = directory.filePath("İstanbul.webm");
        const auto second = directory.filePath("Second.webm");
        {
            RecentVideos videos(directory.path());
            QVERIFY(videos.remember({first, 1234.56789, 4300}, true));
            QVERIFY(videos.remember({second, 18.125, 900}, true));
        }
        RecentVideos restored(directory.path());
        QCOMPARE(restored.count(), 2);
        QCOMPARE(restored.data(restored.index(0), RecentVideos::MediaTitle).toString(), QString("Second"));
        QCOMPARE(restored.find(first)->position, 1234.56789);
        QFile savedFile(directory.filePath("recent-videos.json"));
        QVERIFY(savedFile.open(QIODevice::ReadOnly));
        const auto savedBytes = savedFile.readAll();
        QVERIFY(!savedBytes.contains("subtitle"));
        QVERIFY(!savedBytes.contains("language"));
        QVERIFY(restored.remember({first, 1250.5, 4300}, true));
        QCOMPARE(restored.count(), 2);
        QCOMPARE(restored.data(restored.index(0), RecentVideos::MediaTitle).toString(), QString("İstanbul"));
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
        QCOMPARE(videos.count(), 0);
    }
};
QTEST_GUILESS_MAIN(RecentVideoTests)
#include "recent_videos_tests.moc"
