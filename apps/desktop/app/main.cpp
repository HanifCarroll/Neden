#include "playback_environment.h"
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
#include <QQuickItem>
#include <QColorSpace>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTimer>
#include <QtWebView/QtWebView>

namespace {
bool showsWhiteVideoFrame(QQuickWindow *window) {
    const auto *video = window->findChild<QQuickItem *>("videoSurface");
    if (!video || video->width() < 1 || video->height() < 1)
        return false;
    const auto image = window->grabWindow();
    if (image.isNull())
        return false;
    // Sample the center of the generated white fixture, clear of letterboxing
    // and playback controls. Metadata and a running clock cannot prove rendering.
    for (const qreal vertical : {0.45, 0.5, 0.55}) {
        for (const qreal horizontal : {0.45, 0.5, 0.55}) {
            const auto position = video->mapToScene(QPointF(video->width() * horizontal, video->height() * vertical));
            const QPoint pixel(qRound(position.x() * image.width() / window->width()),
                               qRound(position.y() * image.height() / window->height()));
            if (!image.rect().contains(pixel))
                return false;
            const auto color = image.pixelColor(pixel);
            if (color.red() < 220 || color.green() < 220 || color.blue() < 220)
                return false;
        }
    }
    return true;
}

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
    initializePlaybackEnvironment();
    application.setApplicationName("Neden");
    application.setApplicationVersion(QStringLiteral(NEDEN_VERSION));
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
    commandLineParser.addVersionOption();
    commandLineParser.addOption({"smoke-test", "Render one frame and exit; with a video, verify playback advances (requires --profile)."});
    commandLineParser.addOption({"smoke-test-white-frame", "Also verify visible pixels of the generated white video fixture (requires --smoke-test and a video)."});
    commandLineParser.addOption(
        {"profile", "Use a separate local data directory for development checks.", "directory"});
    commandLineParser.addPositionalArgument("video", "Local video to open.", "[video]");
    commandLineParser.process(application);
    if (commandLineParser.isSet("smoke-test") && !commandLineParser.isSet("profile"))
        commandLineParser.showHelp(2);
    if (commandLineParser.isSet("smoke-test-white-frame") &&
        (!commandLineParser.isSet("smoke-test") || commandLineParser.positionalArguments().isEmpty()))
        commandLineParser.showHelp(2);
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
    if (commandLineParser.isSet("smoke-test")) {
        // Supplying a synthetic video also exercises the installed renderer and
        // playback clock. This mode is isolated from the normal user profile.
        const bool verifyPlayback = !commandLineParser.positionalArguments().isEmpty();
        const bool verifyPixels = commandLineParser.isSet("smoke-test-white-frame");
        QObject::connect(mainWindow, &QQuickWindow::frameSwapped, &application,
                         [&application, &session, &playbackEngine, verifyPlayback, verifyPixels] {
            if (verifyPixels)
                return;
            if (!verifyPlayback || (session.duration() > 0 && session.position() >= 0.2 &&
                                     !playbackEngine.sourceOutputSize().isEmpty()))
                application.exit(0);
        }, Qt::QueuedConnection);
        if (verifyPlayback) {
            QObject::connect(&playbackEngine, &PlaybackEngine::loaded, &application,
                             [&session] { session.setPaused(false); });
        }
        if (verifyPixels) {
            auto *pixelCheck = new QTimer(&application);
            QObject::connect(pixelCheck, &QTimer::timeout, &application,
                             [&application, &session, mainWindow] {
                if (session.position() >= 0.2 && showsWhiteVideoFrame(mainWindow))
                    application.exit(0);
            });
            pixelCheck->start(100);
        }
        QTimer::singleShot(20000, &application, [&application, &session, mainWindow, storage, verifyPixels] {
            qWarning() << "Smoke check timed out; playback state:" << session.playbackState()
                       << "position:" << session.position() << "duration:" << session.duration();
            if (verifyPixels) {
                // Only this explicitly requested synthetic-fixture check captures
                // a failure image, inside its required isolated profile.
                QDir().mkpath(storage);
                mainWindow->grabWindow().save(QDir(storage).filePath("smoke-failure.png"));
            }
            application.exit(4);
        });
        mainWindow->update();
    }
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
