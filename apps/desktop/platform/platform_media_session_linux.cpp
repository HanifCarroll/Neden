#include "platform_media_session.h"
#include "learning_session.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTimer>

namespace {
constexpr auto objectPath = "/org/mpris/MediaPlayer2";
class PlayerBus : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canNext)
    Q_PROPERTY(bool CanGoPrevious READ canPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPlay)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)
public:
    PlayerBus(QObject *owner, LearningSession *session) : QDBusAbstractAdaptor(owner), session(session) {}
    QString playbackStatus() const {
        return !session->hasMedia() ? "Stopped" : session->paused() ? "Paused" : "Playing";
    }
    QDBusObjectPath trackId() const {
        return QDBusObjectPath(
            "/org/mpris/MediaPlayer2/track/t" +
            QString::fromLatin1(
                QCryptographicHash::hash(session->mediaPath().toUtf8(), QCryptographicHash::Sha256).toHex()));
    }
    QVariantMap metadata() const {
        return {{"mpris:trackid", QVariant::fromValue(trackId())},
                {"xesam:title", session->mediaTitle()},
                {"mpris:length", qRound64(session->duration() * 1000000)}};
    }
    double rate() const { return session->playbackSpeed(); }
    void setRate(double value) { session->rate(value); }
    double volume() const { return session->volumeLevel() / 100; }
    void setVolume(double value) { session->volume(value * 100); }
    qlonglong position() const { return qRound64(session->position() * 1000000); }
    double minimumRate() const { return 0.25; }
    double maximumRate() const { return 3; }
    bool canNext() const { return session->queue()->hasNext(); }
    bool canPrevious() const { return session->queue()->hasPrevious(); }
    bool canPlay() const { return session->hasMedia(); }
    bool canSeek() const { return session->seekable(); }
    bool canControl() const { return true; }
public slots:
    void Play() { session->setPlaying(); }
    void Pause() { session->setPaused(true); }
    void PlayPause() { session->toggle(); }
    void Stop() {
        session->setPaused(true);
        session->seek(0);
    }
    void Next() { session->queue()->next(); }
    void Previous() { session->queue()->previous(); }
    void Seek(qlonglong offset) {
        if (canSeek()) {
            session->seek(session->position() + offset / 1000000.0);
            emit Seeked(position());
        }
    }
    void SetPosition(const QDBusObjectPath &track, qlonglong time) {
        if (track.path() == trackId().path() && time >= 0 && time <= session->duration() * 1000000 &&
            canSeek()) {
            session->seek(time / 1000000.0);
            emit Seeked(time);
        }
    }
    void OpenUri(const QString &uri) { session->openVideo(QUrl(uri)); }
signals:
    void Seeked(qlonglong position);

private:
    LearningSession *session;
};
class ApplicationBus : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ supported)
    Q_PROPERTY(bool CanRaise READ supported)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ schemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ mimeTypes)
public:
    ApplicationBus(QObject *owner, QWindow *window) : QDBusAbstractAdaptor(owner), window(window) {}
    bool supported() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return "Neden"; }
    QString desktopEntry() const { return "neden"; }
    QStringList schemes() const { return {"file", "http", "https"}; }
    QStringList mimeTypes() const {
        return {"video/mp4", "video/x-matroska", "video/webm", "video/quicktime"};
    }
public slots:
    void Raise() {
        window->show();
        window->raise();
        window->requestActivate();
    }
    void Quit() { QCoreApplication::quit(); }

private:
    QWindow *window;
};
void releaseInhibition(const QString &path) {
    if (path.isEmpty())
        return;
    auto message = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", path,
                                                  "org.freedesktop.portal.Request", "Close");
    QDBusConnection::sessionBus().asyncCall(message);
}
} // namespace
struct PlatformMediaSession::Native {
    QObject busObject;
    PlayerBus *player = nullptr;
    QString service, inhibition;
    bool wanted = false, pending = false;
};
PlatformMediaSession::PlatformMediaSession(LearningSession *player, QWindow *playerWindow, QObject *parent)
    : QObject(parent), native(std::make_unique<Native>()), session(player), window(playerWindow) {
    native->player = new PlayerBus(&native->busObject, session);
    new ApplicationBus(&native->busObject, window);
    native->service =
        "org.mpris.MediaPlayer2.neden.instance" + QString::number(QCoreApplication::applicationPid());
    auto bus = QDBusConnection::sessionBus();
    bus.registerService(native->service);
    bus.registerObject(objectPath, &native->busObject, QDBusConnection::ExportAdaptors);
    connect(session, &LearningSession::pausedChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::mediaChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::playbackStateChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::durationChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::mediaMetadataChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::speedChanged, this, &PlatformMediaSession::update);
    connect(session, &LearningSession::volumeChanged, this, &PlatformMediaSession::update);
    connect(session->queue(), &MediaQueue::changed, this, &PlatformMediaSession::update);
}
void PlatformMediaSession::update() {
    const bool wanted = session->hasMedia() && !session->paused() && session->playbackState() != "error";
    native->wanted = wanted;
    if (!wanted && !native->inhibition.isEmpty()) {
        releaseInhibition(native->inhibition);
        native->inhibition.clear();
    }
    if (wanted && native->inhibition.isEmpty() && !native->pending) {
        native->pending = true;
        auto request = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop",
                                                      "/org/freedesktop/portal/desktop",
                                                      "org.freedesktop.portal.Inhibit", "Inhibit");
        request.setArguments({QString(), uint(8), QVariantMap{{"reason", "Neden video playback"}}});
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            native->pending = false;
            if (!reply.isError()) {
                if (native->wanted)
                    native->inhibition = reply.value().path();
                else
                    releaseInhibition(reply.value().path());
            }
            watcher->deleteLater();
        });
    }
    auto message =
        QDBusMessage::createSignal(objectPath, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    message.setArguments({QString("org.mpris.MediaPlayer2.Player"),
                          QVariantMap{{"PlaybackStatus", native->player->playbackStatus()},
                                      {"Metadata", native->player->metadata()},
                                      {"Rate", session->playbackSpeed()},
                                      {"Volume", session->volumeLevel() / 100},
                                      {"CanGoNext", session->queue()->hasNext()},
                                      {"CanGoPrevious", session->queue()->hasPrevious()}},
                          QStringList{}});
    QDBusConnection::sessionBus().send(message);
}
PlatformMediaSession::~PlatformMediaSession() {
    releaseInhibition(native->inhibition);
    auto bus = QDBusConnection::sessionBus();
    bus.unregisterObject(objectPath);
    bus.unregisterService(native->service);
}
#include "platform_media_session_linux.moc"
