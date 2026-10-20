#include "shortcut_map.h"
#include "subtitle_appearance.h"
#include "credentials.h"
#include "platform_media_session.h"
#include "desktop_application.h"
#include "recent_videos.h"
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include "fullscreen_controller.h"
#include "learning_session.h"
#include "mpv_playback.h"
#include "thumbnail_service.h"
#include <QQuickImageProvider>
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

namespace {
class PreviewImages : public QQuickImageProvider {
public:
    explicit PreviewImages(ThumbnailService *service)
        : QQuickImageProvider(QQuickImageProvider::Image), service(service) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        const auto image = service->currentImage();
        if (size)
            *size = image.size();
        return image;
    }

private:
    ThumbnailService *service;
};
} // namespace
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
    CredentialStore credentials;
    StudyServices study(storage, [&](const QString &endpoint) { return credentials.read(endpoint); });
    study.settings()->setCredentialAuthorizer([&](const QString &endpoint, CredentialStore::Completion completed) { credentials.authorize(endpoint, std::move(completed)); });
    study.settings()->setCredentialWriter([&](const QString &endpoint, QByteArray key, CredentialStore::Completion completed) { credentials.save(endpoint, std::move(key), std::move(completed)); });
    study.subtitles()->setSubtitleLoginReader([&] { return credentials.subtitleLogin(); });
    LearningSession session(&playbackEngine);
    session.setStudyServices(&study);
    session.setRecentVideos(&recentVideos);
    session.initializePreferences(storage);
    QObject::connect(&application, &DesktopApplication::fileOpened, &session, &LearningSession::openVideo);
    QObject::connect(&application, &QGuiApplication::aboutToQuit, &session, [&] {
        session.setPaused(true);
        session.checkpoint();
        study.explanation()->stop();
    });
    session.cueModel()->setSearchLanguage(CueModel::Turkish);
    FullscreenController fullscreen;
    ThumbnailService thumbnails;
    ShortcutMap keymap;
    keymap.load(storage);
    QQmlApplicationEngine qmlEngine;
    qmlEngine.addImageProvider("preview", new PreviewImages(&thumbnails));
    qmlEngine.setInitialProperties({{"keymap", QVariant::fromValue(&keymap)}, {"thumbnails", QVariant::fromValue(&thumbnails)},
                                    {"appearance", QVariant::fromValue(&subtitleAppearance)},
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
    PlatformMediaSession platformMedia(&session, mainWindow);
    mainWindow->raise();
    mainWindow->requestActivate();
    if (!commandLineParser.positionalArguments().isEmpty()) {
        QList<QUrl> urls;
        for (const auto &argument : commandLineParser.positionalArguments())
            urls << QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile);
        session.openVideos(urls);
    }
    return application.exec();
}
