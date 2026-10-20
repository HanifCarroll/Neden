#include "video_geometry.h"
#include <QtTest>

class VideoGeometryTests : public QObject {
    Q_OBJECT
private slots:
    void appliesDisplayRotation() {
        QCOMPARE(rotatedVideoSize({1920, 1080}, 0), QSize(1920, 1080));
        QCOMPARE(rotatedVideoSize({1920, 1080}, 90), QSize(1080, 1920));
        QCOMPARE(rotatedVideoSize({1920, 1080}, 180), QSize(1920, 1080));
        QCOMPARE(rotatedVideoSize({1920, 1080}, 270), QSize(1080, 1920));
    }
    void rejectsInvalidMetadata() {
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, rotatedVideoSize({}, 0));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, rotatedVideoSize({1920, 1080}, 45));
    }
};
QTEST_GUILESS_MAIN(VideoGeometryTests)
#include "video_geometry_tests.moc"
