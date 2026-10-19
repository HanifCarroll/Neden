#include "subtitle_appearance.h"
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
#include <QNetworkReply>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QtTest>
#include <QtWebView/QtWebView>
#include <cmath>
#include <memory>

#ifdef Q_OS_MACOS
bool nativeMenusAreAvailable();
bool clickNativeInspectorButton(QQuickWindow *window, unsigned index);
#endif

class NativeUiChecks : public QObject {
    Q_OBJECT
private:
    QTemporaryDir profile;
    std::unique_ptr<SubtitleAppearance> appearance;
    std::unique_ptr<MpvPlayback> playback;
    std::unique_ptr<RecentVideos> recent;
    std::unique_ptr<StudyTools> study;
    std::unique_ptr<LearningSession> session;
    std::unique_ptr<FullscreenController> fullscreen;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window = nullptr;
    double stoppedAt = 0;

    void createWindow() {
        appearance = std::make_unique<SubtitleAppearance>(profile.path());
        playback = std::make_unique<MpvPlayback>();
        recent = std::make_unique<RecentVideos>(profile.path());
        study = std::make_unique<StudyTools>(profile.path(), [](const QString &endpoint) {
            return QUrl(endpoint).host() == "127.0.0.1" ? QByteArray("synthetic-test-key") : QByteArray();
        });
        session = std::make_unique<LearningSession>(playback.get());
        session->setStudyTools(study.get());
        session->setRecentVideos(recent.get());
        fullscreen = std::make_unique<FullscreenController>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->setInitialProperties({{"appearance", QVariant::fromValue(appearance.get())},{"session", QVariant::fromValue(session.get())},
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

    void chooseFile(const char *pickerName, const QString &path, const char *buttonName = nullptr) {
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
        if (buttonName)
            click(buttonName);
        else
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
        QDir().mkpath(qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS"));
        createWindow();
    }

    void openAndDisplayTwoIndependentlySelectedTracks() {

        // 1. Exercise the production pickers and real native playback on a visible window.
        chooseFile("videoPicker", qEnvironmentVariable("NEDEN_CHECK_VIDEO"));
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        session->setPaused(true);
        chooseFile("primaryPicker", qEnvironmentVariable("NEDEN_CHECK_PRIMARY"), "loadPrimarySubtitlesButton");
        chooseFile("secondaryPicker", qEnvironmentVariable("NEDEN_CHECK_SECONDARY"), "loadSecondarySubtitlesButton");
        QVERIFY(!session->model.cues.isEmpty());
        QVERIFY(session->translationCueCount() > 0);

        // 2. Find an overlap without printing or copying commercial subtitle text.
        QFile secondaryFile(qEnvironmentVariable("NEDEN_CHECK_SECONDARY"));
        QVERIFY(secondaryFile.open(QIODevice::ReadOnly));
        const auto secondary = parseSrt(QString::fromUtf8(secondaryFile.readAll()));
        double target = cueAt(session->model.cues, 195000) >= 0 && cueAt(secondary, 195000) >= 0 ? 195 : -1;
        if (target < 0) {
            for (const auto &primaryCue : session->model.cues) {
                if (primaryCue.startMs < 190000) continue;

                for (const auto &secondaryCue : secondary) {
                    const auto start = qMax(primaryCue.startMs, secondaryCue.startMs);
                    const auto end = qMin(primaryCue.endMs, secondaryCue.endMs);
                    if (end - start > 500) { target = (start + 200) / 1000.0; break; }
                }
                if (target >= 0) break;
            }
        }
        QVERIFY(target >= 0);
        session->seek(target);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(session->position() - target) < 0.1, 10000);
        QVERIFY(!session->subtitle().isEmpty());
        QVERIFY(!session->translation().isEmpty());
        QTest::qWait(300);
        const auto earlyArtifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!earlyArtifacts.isEmpty()) window->grabWindow().save(earlyArtifacts + "/player-layout.png");
        for (const auto &name : {"primarySubtitleText", "secondarySubtitleText"}) {
            auto *text = visualItem(window->contentItem(), name);
            QVERIFY(text);
            qInfo("Subtitle geometry: %s visible=%d width=%.1f height=%.1f", name, text->isVisible(), text->width(), text->height());
            QTRY_VERIFY(text->isVisible() && text->width() > 0 && text->height() > 0);
            QVERIFY(!text->property("text").toString().isEmpty());
        }
        QVERIFY(!window->grabWindow().isNull());
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QDir().mkpath(artifacts);
            QVERIFY(window->grabWindow().save(artifacts + "/player.png"));
        }
        stoppedAt = session->position();
        session->checkpoint();

        // 3. Settings and the recent list use real QML controls and preserve the paused position.
#ifdef Q_OS_MACOS
        QVERIFY(nativeMenusAreAvailable());
        QVERIFY(clickNativeInspectorButton(window, 1));
        QTRY_COMPARE(window->property("inspectorTab").toInt(), 1);
        QVERIFY(clickNativeInspectorButton(window, 0));
        QTRY_COMPARE(window->property("inspectorTab").toInt(), 0);
#endif
        QTest::keyClick(window, Qt::Key_Comma, Qt::ControlModifier);
        auto *settings = window->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        if (!artifacts.isEmpty()) {
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(artifacts + "/settings.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        QTRY_VERIFY(!settings->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
        QVERIFY(window->property("showRecent").toBool());
    }

    void speedSelectionPreservesPlayerShortcutsAndTextInput() {
        window->setProperty("showRecent", false);
        window->setProperty("inspectorTab", 0);
        session->setPaused(true);

        // 1. Select a speed with the mouse, then use the ordinary player shortcuts.
        click("speedChoice");
        click("speedChoiceOption2");
        auto *speed = visualItem(window->contentItem(), "speedChoice");
        QVERIFY(speed);
        auto *popup = speed->property("popup").value<QObject *>();
        QVERIFY(popup);
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(speed->property("currentIndex").toInt(), 2);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!session->paused());
        QVERIFY(!popup->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(session->paused());
        const double beforeSeek = session->position();
        QTest::keyClick(window, Qt::Key_Right);
        QTRY_VERIFY(std::abs(session->position() - beforeSeek - 5) < 0.15);
        QTest::keyClick(window, Qt::Key_Left);
        QTRY_VERIFY(std::abs(session->position() - beforeSeek) < 0.15);
        const double beforeMute = session->volumeLevel();
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(session->volumeLevel(), 0);
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(session->volumeLevel(), beforeMute);

        // 2. A closed, keyboard-focused speed control also treats Space as play/pause.
        speed->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!session->paused());
        QVERIFY(!popup->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(session->paused());

        // 3. Single-letter and Space shortcuts must not consume text in search fields.
        click("transcriptSearchButton");
        auto *search = visualItem(window->contentItem(), "transcriptSearchField");
        QVERIFY(search && search->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_M);
        QTest::keyClick(window, Qt::Key_Space);
        QTest::keyClick(window, Qt::Key_M);
        QCOMPARE(search->property("text").toString(), QString("m m"));
        QCOMPARE(session->volumeLevel(), beforeMute);
        QVERIFY(session->paused());
        click("transcriptSearchButton");
        session->rate(1);
        QVERIFY(speed->setProperty("currentIndex", 1));
    }

    void subtitleControlsPlayOneCueThenPauseOnTheRealAdapter() {
        window->setProperty("showRecent", false);
        session->seek(stoppedAt);
        QTRY_VERIFY(std::abs(session->position() - stoppedAt) < 0.1);
        const int initial = session->activeCue();
        QVERIFY(initial > 0 && initial + 1 < session->model.cues.size());
        session->setPaused(false);
        click("previousSubtitleButton");
        QTRY_VERIFY(std::abs(session->position() - session->model.cues[initial - 1].startMs / 1000.0) < 0.1);
        QVERIFY(!session->paused());
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        QTest::keyClick(window, Qt::Key_Right, Qt::ControlModifier);
        QTRY_VERIFY(std::abs(session->position() - session->model.cues[initial].startMs / 1000.0) < 0.1);
        QVERIFY(!session->paused());
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        QTest::keyClick(window, Qt::Key_Left, Qt::ControlModifier);
        QTRY_VERIFY(std::abs(session->position() - session->model.cues[initial - 1].startMs / 1000.0) < 0.1);
        QVERIFY(!session->paused());
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        click("nextSubtitleButton");
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), 15000);
        QVERIFY(session->paused());
        click("replaySubtitleButton");
        QVERIFY(session->replaying());
        QVERIFY(!session->paused());
        const auto cueDuration = session->model.cues[initial].endMs - session->model.cues[initial].startMs;
        QTRY_VERIFY_WITH_TIMEOUT(!session->replaying(), int(cueDuration) + 10000);
        QVERIFY(session->paused());
        QTRY_VERIFY(std::abs(session->position() - session->model.cues[initial].endMs / 1000.0) < 0.15);
        session->seek(stoppedAt);
        QTRY_VERIFY(std::abs(session->position() - stoppedAt) < 0.1);
        session->setPaused(true);
        session->checkpoint();
    }

    void fullscreenAndCompactWindowKeepControlsAvailable() {
        window->setProperty("showRecent", false);
        if (!fullscreen->nativeMode()) {
            fullscreen->toggle();
            QTRY_VERIFY(fullscreen->active());
            QTest::qWait(200);
            QVERIFY(visualItem(window->contentItem(), "volumeSlider")->isVisible());
            fullscreen->toggle();
            QTRY_VERIFY(!fullscreen->active());
        }
        const auto originalSize = window->size();
        window->resize(900, 540);
        QTest::qWait(200);
        for (const auto &name : {"previousSubtitleButton", "nextSubtitleButton", "replaySubtitleButton", "volumeSlider", "speedChoice"}) {
            auto *item = visualItem(window->contentItem(), name);
            QVERIFY(item && item->isVisible());
            const auto bounds = item->mapRectToScene(item->boundingRect());
            QVERIFY(bounds.left() >= 0 && bounds.right() <= window->width());
            QVERIFY(bounds.top() >= 0 && bounds.bottom() <= window->height());
        }
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) QVERIFY(window->grabWindow().save(artifacts + "/compact.png"));
        window->resize(originalSize);
    }

    void packagedHttpsBackendConnectsWithoutCredentials() {
        QVERIFY(QSslSocket::supportsSsl());
        QVERIFY(QSslSocket::availableBackends().contains("securetransport"));
        QNetworkAccessManager network;
        QNetworkRequest request(QUrl("https://api.deepseek.com/models"));
        request.setTransferTimeout(15000);
        auto *reply = network.get(request);
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait(20000));
        // An unauthenticated API response proves TLS works; no paid request is sent.
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
        QVERIFY(reply->error() != QNetworkReply::SslHandshakeFailedError);
        reply->deleteLater();
    }

    void volumeSelectionAndExplanationUseTheActualControls() {
        // 1. Move the real slider and verify the session, then restore the paused video.
        window->setProperty("showRecent", false);
        auto *volume = visualItem(window->contentItem(), "volumeSlider");
        QVERIFY(volume);
        QVERIFY(volume->isVisible());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            volume->mapToScene(QPointF(volume->width() * 0.35, volume->height() / 2)).toPoint());
        QVERIFY(session->volumeLevel() > 20 && session->volumeLevel() < 50);
        session->volume(0);

        // 2. Open the actual speed popup and verify the highlighted row remains readable.
        click("speedChoice");
        QTRY_VERIFY(visualItem(window->contentItem(), "speedChoiceOption1"));
        auto *option = visualItem(window->contentItem(), "speedChoiceOption1");
        QVERIFY(option->property("highlighted").toBool());
        auto *label = option->property("contentItem").value<QQuickItem *>();
        QVERIFY(label);
        QCOMPARE(label->property("color").value<QColor>(), QColor("#10151b"));
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) QVERIFY(window->grabWindow().save(artifacts + "/highlight.png"));
        auto *speed = visualItem(window->contentItem(), "speedChoice");
        auto *popup = speed->property("popup").value<QObject *>();
        QVERIFY(popup && QMetaObject::invokeMethod(popup, "close"));

        // 3. Double-click a synthetic Turkish word and serve an incremental answer locally.
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("sent").toBool()) return;

                socket->setProperty("sent", true);
                ++requests;
                if (requests == 1) {
                    socket->write("HTTP/1.1 429 Too Many Requests\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }

                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                socket->write("data: {\"choices\":[{\"delta\":{\"content\":\"### Natural meaning\\nLight\"}}]}\n\n");
                QTimer::singleShot(250, socket, [socket] {
                    socket->write("data: [DONE]\n\n");
                    socket->disconnectFromHost();
                });
            });
        });
        QVERIFY(study->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock", "https://example.org/%s", false));
        const auto start = qRound64(session->position() * 1000) - 500;
        session->replaceCues({{start, start + 10000, "ışık", {}}});
        QTest::qWait(250);
        auto *subtitle = visualItem(window->contentItem(), "primarySubtitleText");
        QVERIFY(subtitle && subtitle->isVisible());
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier,
            subtitle->mapToScene(QPointF(subtitle->width() / 2, subtitle->height() / 2)).toPoint());
        QTRY_COMPARE(session->selection(), QString("ışık"));
        QTRY_COMPARE_WITH_TIMEOUT(study->state(), QString("failed"), 5000);
        QVERIFY(study->message().contains("allowance"));
        QTest::qWait(300);
        QCOMPARE(requests, 1);
        if (!artifacts.isEmpty()) QVERIFY(window->grabWindow().save(artifacts + "/network-error.png"));
        click("explainButton");
        QTRY_COMPARE_WITH_TIMEOUT(study->state(), QString("complete"), 5000);
        QCOMPARE(requests, 2);
        QCOMPARE(study->answer(), QString("### Natural meaning\nLight"));
        QCOMPARE(window->property("inspectorTab").toInt(), 1);
        auto *meaning = window->findChild<QObject *>("meaningLoader");
        QVERIFY(meaning && meaning->property("active").toBool());
        if (!artifacts.isEmpty()) QVERIFY(window->grabWindow().save(artifacts + "/explanation.png"));

        // 4. Draft highlight changes leave the completed phrase and answer stable.
        auto *subject = visualItem(window->contentItem(), "explanationSubject");
        QVERIFY(subject);
        QCOMPARE(subject->property("text").toString(), QString("ışık"));
        session->select("ış");
        QTest::qWait(100);
        QCOMPARE(subject->property("text").toString(), QString("ışık"));
        QCOMPARE(study->answer(), QString("### Natural meaning\nLight"));
        QCOMPARE(requests, 2);
        click("transcriptTab");
        QTRY_VERIFY(session->selection().isEmpty());
    }

    void closeReopenResumeAndRemoveThroughTheUi() {

        // 1. Destroy the whole window and service graph, then reconstruct from saved disk state.
        destroyWindow();
        createWindow();
        QCOMPARE(recent->count(), 1);
        QVERIFY(!session->hasMedia());
        const auto artifacts = qEnvironmentVariable("NEDEN_CHECK_ARTIFACTS");
        if (!artifacts.isEmpty()) {
            QTest::qWait(200);
            QVERIFY(window->grabWindow().save(artifacts + "/recent-videos.png"));
        }
        click("resumeVideoButton");
        QTRY_VERIFY_WITH_TIMEOUT(session->duration() > 0, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(session->position() - stoppedAt) < 1, 10000);
        session->setPaused(true);
        QVERIFY(session->subtitle().isEmpty());
        QVERIFY(session->translation().isEmpty());
        QVERIFY(!visualItem(window->contentItem(), "primarySubtitleText")->isVisible());
        QVERIFY(!visualItem(window->contentItem(), "secondarySubtitleText")->isVisible());

        // 2. Remove the playing video, close, and reopen without silently recreating its entry.
        QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
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
