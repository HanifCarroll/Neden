#pragma once
#include "playback_engine.h"
#include <QSettings>
#include <memory>

// Durable choices are independent from history. Clearing history also clears per-media choices.
class PlayerPreferences : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Preferences belong to the player")
    Q_PROPERTY(double audioDelay READ audioDelay WRITE setAudioDelay NOTIFY changed)
    Q_PROPERTY(QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY changed)
    Q_PROPERTY(int rotation READ rotation WRITE setRotation NOTIFY changed)
    Q_PROPERTY(QString aspect READ aspect WRITE setAspect NOTIFY changed)
    Q_PROPERTY(bool fill READ fill WRITE setFill NOTIFY changed)
    Q_PROPERTY(bool deinterlace READ deinterlace WRITE setDeinterlace NOTIFY changed)
    Q_PROPERTY(bool nightMode READ nightMode WRITE setNightMode NOTIFY changed)
    Q_PROPERTY(bool rememberSpeed READ rememberSpeed WRITE setRememberSpeed NOTIFY changed)
    Q_PROPERTY(bool rememberHistory READ rememberHistory WRITE setRememberHistory NOTIFY changed)
    Q_PROPERTY(QString subtitleEncoding READ subtitleEncoding WRITE setSubtitleEncoding NOTIFY changed)
public:
    explicit PlayerPreferences(PlaybackEngine *engine, QObject *parent = nullptr);
    void initialize(const QString &directory);
    void beginMedia(const QString &path);
    void applyMedia();
    double savedVolume() const;
    double savedSpeed() const;
    void saveVolume(double value);
    void saveSpeed(double value);
    double audioDelay() const { return delay; }
    void setAudioDelay(double value);
    QString audioDevice() const { return device; }
    void setAudioDevice(const QString &value);
    int rotation() const { return rotationDegrees; }
    void setRotation(int value);
    QString aspect() const { return aspectRatio; }
    void setAspect(const QString &value);
    bool fill() const { return fillPicture; }
    void setFill(bool value);
    bool deinterlace() const { return deinterlacePicture; }
    void setDeinterlace(bool value);
    bool nightMode() const { return compressAudio; }
    void setNightMode(bool value);
    bool rememberSpeed() const;
    void setRememberSpeed(bool value);
    bool rememberHistory() const;
    void setRememberHistory(bool value);
    QString subtitleEncoding() const;
    void setSubtitleEncoding(const QString &value);
    Q_INVOKABLE void chooseAudio(const QString &id);
    Q_INVOKABLE void resetPicture();
    Q_INVOKABLE void clearMedia();
signals:
    void changed();
private:
    QVariant read(const QString &key, const QVariant &fallback = {}) const;
    void write(const QString &key, const QVariant &value);
    QString mediaKey(const QString &key) const;
    PlaybackEngine *engine;
    std::unique_ptr<QSettings> settings;
    QString mediaGroup;
    QString device = "auto", aspectRatio = "-1";
    double delay = 0;
    int rotationDegrees = 0;
    bool fillPicture = false, deinterlacePicture = false, compressAudio = false;
};
