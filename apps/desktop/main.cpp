#include "credentials.h"
#include "fullscreen_controller.h"
#include "learning_session.h"
#include "mpv_playback.h"
#include <QColorSpace>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QtWebView/QtWebView>

int main(int argc, char **argv) {

    // 1. Configure the OpenGL video surface and native text before creating windows.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QSurfaceFormat format;
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);
    QtWebView::initialize();
    QGuiApplication application(argc, argv);
    // Keep the existing data directory so saved cards and settings survive the rename.
    application.setApplicationName("Neden Qt Lab");
    application.setApplicationDisplayName("Neden");
    QQuickStyle::setStyle("Basic");
    for (const auto &weight : {"Regular", "Medium", "SemiBold"}) {
        QFontDatabase::addApplicationFont(QString(":/fonts/Inter-%1.otf").arg(weight));
    }
    application.setFont(QFont("Inter"));

    // 2. Keep playback alive until QML and its rendering resources have been released.
    MpvPlayback playback;
    StudyTools study(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation), readSavedCredential);
    study.setCredentialAuthorizer(authorizeSavedCredential);
    study.setSubtitleLoginReader(readSavedSubtitleLogin);
    LearningSession session(&playback);
    session.setStudyTools(&study);
    session.cueModel()->setSearchLanguage(CueModel::Turkish);
    FullscreenController fullscreen;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"session", QVariant::fromValue(&session)},
        {"playback", QVariant::fromValue(&playback)},
        {"fullscreenController", QVariant::fromValue(&fullscreen)}});
    engine.loadFromModule("Neden", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 3;
    }

    // 3. Attach native window controls after the main window exists.
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    fullscreen.attach(window);
    window->raise();
    window->requestActivate();
    return application.exec();
}
