#include "video_render_lifetime.h"
#include <QtTest>

class VideoRenderLifetimeTests : public QObject {
    Q_OBJECT
private slots:
    void idleHasNoRenderer() {
        VideoRenderLifetime lifetime;
        QVERIFY(!lifetime.available());
        QVERIFY(!lifetime.owns(0));
    }

    void destroyedRendererRejectsQueuedReadiness() {
        VideoRenderLifetime lifetime;
        const auto token = lifetime.activate();
        QVERIFY(lifetime.owns(token));
        lifetime.release(token);
        QVERIFY(!lifetime.owns(token));
        QVERIFY(!lifetime.available());
    }

    void oldRendererCannotDetachReplacement() {
        VideoRenderLifetime lifetime;
        const auto previous = lifetime.activate();
        lifetime.release(previous);
        const auto replacement = lifetime.activate();
        QVERIFY(replacement != previous);
        lifetime.release(previous);
        QVERIFY(lifetime.owns(replacement));
        QVERIFY(!lifetime.owns(previous));
    }
};

QTEST_GUILESS_MAIN(VideoRenderLifetimeTests)
#include "video_render_lifetime_tests.moc"
