#include "fullscreen_controller.h"
#include "learning_session.h"
#include "mpv_playback.h"
#include "recent_videos.h"
#include <QColorSpace>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>
#include <QtWebView/QtWebView>
#include <cmath>
#include <memory>

class NativeUiChecks : public QObject {
    Q_OBJECT
private:
    QTemporaryDir profile;
    std::unique_ptr<MpvPlayback> playback;
    std::unique_ptr<RecentVideos> recent;
    std::unique_ptr<StudyTools> study;
    std::unique_ptr<LearningSession> session;
    std::unique_ptr<FullscreenController> fullscreen;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window = nullptr;
    double stoppedAt = 0;

    void createWindow() {
        playback = std::make_unique<MpvPlayback>();
        recent = std::make_unique<RecentVideos>(profile.path());
        study = std::make_unique<StudyTools>(profile.path(), [](const QString &) { return QByteArray{}; });
        session = std::make_unique<LearningSession>(playback.get());
        session->setStudyTools(study.get());
        session->setRecentVideos(recent.get());
        fullscreen = std::make_unique<FullscreenController>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->setInitialProperties({{"session", QVariant::fromValue(session.get())},
            {"playback", QVariant::fromValue(playback.get())},
            {"recentVideos", QVariant::fromValue(recent.get())},
            {"fullscreenController", QVariant::fromValue(fullscreen.get())}});
        engine->loadFromModule("Neden", "Main");
        QVERIFY(!engine->rootObjects().isEmpty());
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
        QVERIFY(window);
        fullscreen->attach(window);
        window->raise();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }

    void destroyWindow() {
        if (window) window->close();
        engine.reset();
        fullscreen.reset();
        session.reset();
        study.reset();
        recent.reset();
        playback.reset();
        window = nullptr;
    }

    QQuickItem *visualItem(QQuickItem *parent, const QString &name) {
        if (parent->objectName() == name) return parent;

        for (auto *child : parent->childItems()) {
            if (auto *found = visualItem(child, name)) return found;
        }
        return nullptr;
    }

    void click(const char *name) {
        QTRY_VERIFY(visualItem(window->contentItem(), QString::fromUtf8(name)));
        auto *item = visualItem(window->contentItem(), QString::fromUtf8(name));
        QVERIFY2(item, name);
        QVERIFY(item->isVisible());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        QCoreApplication::processEvents();
    }

    void chooseFile(const char *pickerName, const QString &path) {
        auto *picker = window->findChild<QObject *>(pickerName);
        QVERIFY(picker);
        // Qt's own picker is controllable without granting OS Accessibility access.
        constexpr int dontUseNativeDialog = 0x8;
        QVERIFY(picker->setProperty("options", dontUseNativeDialog));
        bool accepted = false;
        QTimer::singleShot(500, picker, [picker, path, &accepted] {
            QVERIFY(picker->property("visible").toBool());
            QVERIFY(picker->setProperty("selectedFile", QUrl::fromLocalFile(path)));
            QVERIFY(QMetaObject::invokeMethod(picker, "accepted"));
            QVERIFY(QMetaObject::invokeMethod(picker, "close"));
            accepted = true;
        });
        QVERIFY(QMetaObject::invokeMethod(picker, "open"));
        QTRY_VERIFY_WITH_TIMEOUT(accepted, 10000);
        QTRY_VERIFY(!picker->property("visible").toBool());
        qInfo("Completed file picker: %s", pickerName);
    }

private slots:
    void initTestCase() {
        for (const auto &name : {"NEDEN_CHECK_VIDEO", "NEDEN_CHECK_PRIMARY", "NEDEN_CHECK_SECONDARY"}) {
            QVERIFY2(QFile::exists(qEnvironmentVariable(name)), name);
        }
        createWindow();
    }

    void openAndDisplayTwoIndependentlySelectedTracks() {

        // 1. Exercise the production pickers and real native playback on a visible window.
        chooseFile("videoPicker", qEnvironmentVariable("NEDEN_CHECK_VIDEO"));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        session->setPaused(true);
        chooseFile("primaryPicker", qEnvironmentVariable("NEDEN_CHECK_PRIMARY"));
        chooseFile("secondaryPicker", qEnvironmentVariable("NEDEN_CHECK_SECONDARY"));
        QVERIFY(!session->model.cues.isEmpty());
        QVERIFY(session->translationCueCount() > 0);

        // 2. Find an overlap without printing or copying commercial subtitle text.
        QFile secondaryFile(qEnvironmentVariable("NEDEN_CHECK_SECONDARY"));
        QVERIFY(secondaryFile.open(QIODevice::ReadOnly));
        const auto secondary = parseSrt(QString::fromUtf8(secondaryFile.readAll()));
        double target = -1;
        for (const auto &primaryCue : session->model.cues) {
            if (primaryCue.startMs < 300000) continue;
            for (const auto &secondaryCue : secondary) {
                const auto start = qMax(primaryCue.startMs, secondaryCue.startMs);
                const auto end = qMin(primaryCue.endMs, secondaryCue.endMs);
                if (end - start > 500) { target = (start + 200) / 1000.0; break; }
            }
            if (target >= 0) break;
        }
        QVERIFY(target >= 0);
        session->seek(target);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(session->position() - target) < 0.1, 10000);
        QVERIFY(!session->subtitle().isEmpty());
        QVERIFY(!session->translation().isEmpty());
        for (const auto &name : {"primarySubtitleText", "secondarySubtitleText"}) {
            auto *text = window->findChild<QQuickItem *>(name);
            QVERIFY(text && text->isVisible() && text->width() > 0 && text->height() > 0);
            QVERIFY(!text->property("text").toString().isEmpty());
        }
        QVERIFY(!window->grabWindow().isNull());
        stoppedAt = session->position();
        session->checkpoint();

        // 3. Settings and the recent list use real QML controls and preserve the paused position.
        click("settingsButton");
        auto *settings = window->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        QTRY_VERIFY(!settings->property("opened").toBool());
        click("recentVideosButton");
        QVERIFY(window->property("showRecent").toBool());
    }

    void closeReopenResumeAndRemoveThroughTheUi() {

        // 1. Destroy the whole window and service graph, then reconstruct from saved disk state.
        destroyWindow();
        createWindow();
        QCOMPARE(recent->count(), 1);
        QVERIFY(!session->hasMedia());
        click("resumeVideoButton");
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(session->position() - stoppedAt) < 1, 10000);
        session->setPaused(true);
        QVERIFY(session->subtitle().isEmpty());
        QVERIFY(session->translation().isEmpty());
        QVERIFY(!visualItem(window->contentItem(), "primarySubtitleText")->isVisible());
        QVERIFY(!visualItem(window->contentItem(), "secondarySubtitleText")->isVisible());

        // 2. Remove the playing video, close, and reopen without silently recreating its entry.
        click("recentVideosButton");
        click("removeRecentButton");
        QCOMPARE(recent->count(), 0);
        session->checkpoint();
        destroyWindow();
        createWindow();
        QCOMPARE(recent->count(), 0);
        QVERIFY(QFile::exists(qEnvironmentVariable("NEDEN_CHECK_VIDEO")));
    }

    void cleanupTestCase() { destroyWindow(); }
};

int main(int argc, char **argv) {
    qputenv("QSG_RENDER_LOOP", "basic");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QSurfaceFormat format;
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);
    QtWebView::initialize();
    QGuiApplication application(argc, argv);
    application.setApplicationName("Neden UI checks");
    application.setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle("Basic");
    for (const auto &weight : {"Regular", "Medium", "SemiBold"}) {
        QFontDatabase::addApplicationFont(QString(":/fonts/Inter-%1.otf").arg(weight));
    }
    NativeUiChecks checks;
    QTimer::singleShot(0, &application, [&] {
        application.exit(QTest::qExec(&checks, argc, argv));
    });
    return application.exec();
}
#include "native_ui_checks.moc"
