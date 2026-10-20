#pragma once
#include <QObject>
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <QtQml/qqmlregistration.h>

// One lazy worker, one pending request. Network media is intentionally excluded.
class ThumbnailWorker : public QThread {
    Q_OBJECT
public:
    explicit ThumbnailWorker(QObject *parent = nullptr) : QThread(parent) {}
    ~ThumbnailWorker() override;
    void request(const QString &path, double seconds, quint64 generation);
signals:
    void ready(QImage image, quint64 generation);

protected:
    void run() override;

private:
    QMutex mutex;
    QWaitCondition pending;
    QString mediaPath;
    double position = 0;
    quint64 generation = 0;
    bool requested = false, stopping = false;
};

class ThumbnailService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Thumbnails belong to the application")
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY changed)
public:
    explicit ThumbnailService(QObject *parent = nullptr);
    Q_INVOKABLE void request(const QString &path, double seconds);
    Q_INVOKABLE void clear();
    QString imageUrl() const {
        return image.isNull() ? QString() : "image://preview/" + QString::number(generation);
    }
    QImage currentImage() const { return image; }
signals:
    void changed();

private:
    ThumbnailWorker worker;
    QImage image;
    QString lastPath;
    int lastSecond = -1;
    quint64 generation = 0;
};
