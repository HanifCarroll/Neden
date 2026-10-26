#include "playback_environment.h"
#include "thumbnail_service.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class RecordingThumbnailWorker : public ThumbnailWorker {
public:
    QList<ThumbnailRequest> requests;
    void request(const ThumbnailRequest &request) override { requests.append(request); }
    void cancel() override {}
    void finish(const ThumbnailRequest &request, Qt::GlobalColor color = Qt::red) {
        QImage image(320, 180, QImage::Format_RGBX8888);
        image.fill(color);
        emit ready(image, request);
    }
};
class ThumbnailTests : public QObject {
    Q_OBJECT
private slots:
    void bundledDecoderRendersLocalVideo() {
        const auto path = QFINDTESTDATA("fixtures/white.mp4");
        QVERIFY(!path.isEmpty());
        ThumbnailService service;
        service.request(path, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!service.currentImage().isNull(), 7000);
        QCOMPARE(service.currentImage().size(), QSize(320, 180));
        const auto center = service.currentImage().pixelColor(160, 90);
        QVERIFY(center.red() > 225 && center.green() > 225 && center.blue() > 225);
    }
    void immediateThrottledAndRefined() {
        QTemporaryDir directory;
        QFile file(directory.filePath("video"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("fixture"); file.close();
        RecordingThumbnailWorker worker;
        ThumbnailService service(worker);
        service.request(file.fileName(), 1);
        QCOMPARE(worker.requests.size(), 1);
        QVERIFY(!worker.requests.last().precise);
        for (int second = 2; second <= 10; ++second)
            service.request(file.fileName(), second);
        QCOMPARE(worker.requests.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(worker.requests.size() >= 2, 150);
        QCOMPARE(worker.requests.at(1).second, 10);
        QVERIFY(!worker.requests.at(1).precise);
        QTRY_VERIFY_WITH_TIMEOUT(worker.requests.last().precise, 400);
        QCOMPARE(worker.requests.last().second, 10);
    }
    void cacheSurvivesHoverAndRejectsOldResultsAndChangedFiles() {
        QTemporaryDir directory;
        QFile file(directory.filePath("video"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("fixture"); file.close();
        RecordingThumbnailWorker worker;
        ThumbnailService service(worker);
        service.request(file.fileName(), 1);
        const auto first = worker.requests.last();
        worker.finish(first);
        QVERIFY(!service.currentImage().isNull());
        QVERIFY(service.approximate());
        QTRY_VERIFY(worker.requests.last().precise);
        const auto exact = worker.requests.last();
        worker.finish(exact, Qt::green);
        QVERIFY(!service.approximate());
        service.clear();
        QVERIFY(service.currentImage().isNull());
        const auto count = worker.requests.size();
        service.request(file.fileName(), 1);
        QCOMPARE(worker.requests.size(), count);
        QCOMPARE(service.currentImage().pixelColor(0, 0), QColor(Qt::green));
        service.request(file.fileName(), 5);
        worker.finish(first, Qt::blue);
        QCOMPARE(service.currentImage().pixelColor(0, 0), QColor(Qt::green));
        QVERIFY(service.approximate());
        service.clear();
        worker.finish(exact);
        QVERIFY(service.currentImage().isNull());
        QVERIFY(file.open(QIODevice::Append)); file.write("changed"); file.close();
        service.request(file.fileName(), 1);
        QVERIFY(service.currentImage().isNull());
        QVERIFY(worker.requests.last().fileIdentity != first.fileIdentity);
    }
    void invalidOrNetworkMediaNeverDecodes() {
        RecordingThumbnailWorker worker;
        ThumbnailService service(worker);
        service.request("https://example.com/video.mp4", 20);
        service.request("/does/not/exist", 20);
        QVERIFY(worker.requests.isEmpty());
    }
};
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    initializePlaybackEnvironment();
    ThumbnailTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "thumbnail_tests.moc"
