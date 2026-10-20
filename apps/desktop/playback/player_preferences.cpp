#include "player_preferences.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <cmath>

PlayerPreferences::PlayerPreferences(PlaybackEngine *playback, QObject *parent) : QObject(parent), engine(playback) {}
void PlayerPreferences::initialize(const QString &directory) {
    settings = std::make_unique<QSettings>(QDir(directory).filePath("playback.ini"), QSettings::IniFormat);
    device = read("audioDevice", "auto").toString();
    engine->configure(PlaybackEngine::Option::AudioDevice, device);
    engine->configure(PlaybackEngine::Option::SubtitleEncoding, subtitleEncoding());
    emit changed();
}
QVariant PlayerPreferences::read(const QString &key, const QVariant &fallback) const { return settings ? settings->value(key, fallback) : fallback; }
void PlayerPreferences::write(const QString &key, const QVariant &value) {
    if (settings && !key.isEmpty()) settings->setValue(key, value);
    emit changed();
}
QString PlayerPreferences::mediaKey(const QString &key) const { return mediaGroup.isEmpty() ? QString() : mediaGroup + '/' + key; }
void PlayerPreferences::beginMedia(const QString &path) {
    mediaGroup = QFileInfo(path).isAbsolute() && rememberHistory()
        ? "media/" + QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex()) : QString();
    delay = read(mediaKey("audioDelay"), 0).toDouble();
    rotationDegrees = read(mediaKey("rotation"), 0).toInt();
    aspectRatio = read(mediaKey("aspect"), "-1").toString();
    fillPicture = read(mediaKey("fill"), false).toBool();
    deinterlacePicture = read(mediaKey("deinterlace"), false).toBool();
    emit changed();
}
void PlayerPreferences::applyMedia() {
    engine->configure(PlaybackEngine::Option::AudioDelay, delay);
    engine->configure(PlaybackEngine::Option::Rotation, rotationDegrees);
    engine->configure(PlaybackEngine::Option::AspectRatio, aspectRatio);
    engine->configure(PlaybackEngine::Option::Fill, fillPicture);
    engine->configure(PlaybackEngine::Option::Deinterlace, deinterlacePicture);
    engine->configure(PlaybackEngine::Option::AudioTrack, read(mediaKey("audioTrack"), "auto"));
}
double PlayerPreferences::savedVolume() const { return qBound(0.0, read("volume", 60.0).toDouble(), 100.0); }
double PlayerPreferences::savedSpeed() const { return rememberSpeed() ? qBound(0.25, read("speed", 1.0).toDouble(), 3.0) : 1.0; }
void PlayerPreferences::saveVolume(double value) { write("volume", value); }
void PlayerPreferences::saveSpeed(double value) { write("speed", value); }
bool PlayerPreferences::rememberSpeed() const { return read("rememberSpeed", false).toBool(); }
void PlayerPreferences::setRememberSpeed(bool value) { write("rememberSpeed", value); }
bool PlayerPreferences::rememberHistory() const { return read("rememberHistory", true).toBool(); }
void PlayerPreferences::setRememberHistory(bool value) { write("rememberHistory", value); }
QString PlayerPreferences::subtitleEncoding() const { return read("subtitleEncoding", "auto").toString(); }
void PlayerPreferences::setSubtitleEncoding(const QString &value) {
    if (!QStringList{"auto", "UTF-8", "UTF-16", "windows-1254", "windows-1252", "ISO-8859-9"}.contains(value)) return;
    write("subtitleEncoding", value);
    engine->configure(PlaybackEngine::Option::SubtitleEncoding, value);
}
void PlayerPreferences::chooseAudio(const QString &id) { write(mediaKey("audioTrack"), id); engine->configure(PlaybackEngine::Option::AudioTrack, id); }
void PlayerPreferences::setAudioDelay(double value) {
    if (!std::isfinite(value)) return;
    delay = qBound(-60.0, value, 60.0); write(mediaKey("audioDelay"), delay); engine->configure(PlaybackEngine::Option::AudioDelay, delay);
}
void PlayerPreferences::setAudioDevice(const QString &value) { device = value; write("audioDevice", value); engine->configure(PlaybackEngine::Option::AudioDevice, value); }
void PlayerPreferences::setRotation(int value) { rotationDegrees = ((value % 360) + 360) % 360; write(mediaKey("rotation"), rotationDegrees); engine->configure(PlaybackEngine::Option::Rotation, rotationDegrees); }
void PlayerPreferences::setAspect(const QString &value) { if (!QStringList{"-1", "16:9", "4:3", "1:1", "2.35:1"}.contains(value)) return; aspectRatio = value; write(mediaKey("aspect"), value); engine->configure(PlaybackEngine::Option::AspectRatio, value); }
void PlayerPreferences::setFill(bool value) { fillPicture = value; write(mediaKey("fill"), value); engine->configure(PlaybackEngine::Option::Fill, value); }
void PlayerPreferences::setDeinterlace(bool value) { deinterlacePicture = value; write(mediaKey("deinterlace"), value); engine->configure(PlaybackEngine::Option::Deinterlace, value); }
void PlayerPreferences::setNightMode(bool value) { if (compressAudio == value) return; compressAudio = value; engine->configure(PlaybackEngine::Option::NightMode, value); emit changed(); }
void PlayerPreferences::resetPicture() { setRotation(0); setAspect("-1"); setFill(false); setDeinterlace(false); }
void PlayerPreferences::clearMedia() { if (settings) settings->remove("media"); mediaGroup.clear(); emit changed(); }
