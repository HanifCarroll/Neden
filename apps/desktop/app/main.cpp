#include "subtitle_appearance.h"
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
#include <QIcon>
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
    application.setWindowIcon(QIcon(":/branding/neden.png"));
    QQuickStyle::setStyle("Basic");
    for (const auto &fontWeight : {"Regular", "Medium", "SemiBold"}) {
        QFontDatabase::addApplicationFont(QString(":/fonts/Inter-%1.otf").arg(fontWeight));
    }
    application.setFont(QFont("Inter"));

    // 2. Keep playback alive until QML and its rendering resources have been released.
    QCommandLineParser commandLineParser;
    commandLineParser.addHelpOption();
    commandLineParser.addOption(
        {"profile", "Use a separate local data directory for development checks.", "directory"});
    commandLineParser.addPositionalArgument("video", "Local video to open.", "[video]");
    commandLineParser.process(application);
    // Retain existing study data while using Neden for every visible application name.
    const auto storage =
        commandLineParser.isSet("profile")
            ? QDir(commandLineParser.value("profile")).absolutePath()
            : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Neden Qt Lab";
    SubtitleAppearance subtitleAppearance(storage);
    MpvPlayback playbackEngine;
    RecentVideos recentVideos(storage);
    StudyServices study(storage, readSavedCredential);
    study.settings()->setCredentialAuthorizer(authorizeSavedCredential);
    study.settings()->setCredentialWriter(saveCredential);
    study.subtitles()->setSubtitleLoginReader(readSavedSubtitleLogin);
    LearningSession session(&playbackEngine);
    session.setStudyServices(&study);
    session.setRecentVideos(&recentVideos);
    QObject::connect(&application, &DesktopApplication::fileOpened, &session, &LearningSession::openVideo);
    QObject::connect(&application, &QGuiApplication::aboutToQuit, &session, [&] {
        session.setPaused(true);
        session.checkpoint();
        study.explanation()->stop();
    });
    session.cueModel()->setSearchLanguage(CueModel::Turkish);
    FullscreenController fullscreen;
    QQmlApplicationEngine qmlEngine;
    qmlEngine.setInitialProperties({{"appearance", QVariant::fromValue(&subtitleAppearance)},
                                    {"session", QVariant::fromValue(&session)},
                                    {"playback", QVariant::fromValue(&playbackEngine)},
                                    {"recentVideos", QVariant::fromValue(&recentVideos)},
                                    {"fullscreenController", QVariant::fromValue(&fullscreen)}});
    qmlEngine.loadFromModule("Neden", "Main");
    if (qmlEngine.rootObjects().isEmpty()) {
        return 3;
    }

    // 3. Attach native window controls after the main window exists.
    auto *mainWindow = qobject_cast<QQuickWindow *>(qmlEngine.rootObjects().first());
    fullscreen.attach(mainWindow);
    mainWindow->raise();
    mainWindow->requestActivate();
    if (!commandLineParser.positionalArguments().isEmpty()) {
        session.openVideo(QUrl::fromLocalFile(
            QFileInfo(commandLineParser.positionalArguments().first()).absoluteFilePath()));
    }
    return application.exec();
}
