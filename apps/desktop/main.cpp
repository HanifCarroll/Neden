#include "credentials.h"
#include "desktop_application.h"
#include "recent_videos.h"
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
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
#ifdef Q_OS_MACOS
    qputenv("QSG_RENDER_LOOP", "basic");
#endif
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QSurfaceFormat format;
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);
    QtWebView::initialize();
    DesktopApplication application(argc, argv);
    application.setApplicationName("Neden");
    application.setApplicationDisplayName("Neden");
    QQuickStyle::setStyle("Basic");
    for (const auto &weight : {"Regular", "Medium", "SemiBold"}) {
        QFontDatabase::addApplicationFont(QString(":/fonts/Inter-%1.otf").arg(weight));
    }
    application.setFont(QFont("Inter"));

    // 2. Keep playback alive until QML and its rendering resources have been released.
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"profile", "Use a separate local data directory for development checks.", "directory"});
    parser.addPositionalArgument("video", "Local video to open.", "[video]");
    parser.process(application);
    // Retain existing study data while using Neden for every visible application name.
    const auto storage = parser.isSet("profile") ? QDir(parser.value("profile")).absolutePath()
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Neden Qt Lab";
    MpvPlayback playback;
    RecentVideos recentVideos(storage);
    StudyTools study(storage, readSavedCredential);
    study.setCredentialAuthorizer(authorizeSavedCredential);
    study.setSubtitleLoginReader(readSavedSubtitleLogin);
    LearningSession session(&playback);
    session.setStudyTools(&study);
    session.setRecentVideos(&recentVideos);
    QObject::connect(&application, &DesktopApplication::fileOpened, &session, &LearningSession::openVideo);
    QObject::connect(&application, &QGuiApplication::aboutToQuit, &session, [&] {
        session.setPaused(true);
        session.checkpoint();
        study.stop();
    });
    session.cueModel()->setSearchLanguage(CueModel::Turkish);
    FullscreenController fullscreen;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"session", QVariant::fromValue(&session)},
        {"playback", QVariant::fromValue(&playback)},
        {"recentVideos", QVariant::fromValue(&recentVideos)},
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
    if (!parser.positionalArguments().isEmpty()) {
        session.openVideo(QUrl::fromLocalFile(QFileInfo(parser.positionalArguments().first()).absoluteFilePath()));
    }
    return application.exec();
}
