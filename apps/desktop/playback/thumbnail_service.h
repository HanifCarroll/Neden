#pragma once
#include <QObject>
#include <QCache>
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>
#include <QtQml/qqmlregistration.h>
#include <memory>

struct ThumbnailRequest {
    QString path;
    QString fileIdentity;
    int second = 0;
    bool precise = false;
    quint64 generation = 0;
    QString cacheKey() const;
};
Q_DECLARE_METATYPE(ThumbnailRequest)

// A single decoder and a single replaceable pending request bound background work.
class ThumbnailWorker : public QThread {
    Q_OBJECT
public:
    explicit ThumbnailWorker(QObject *parent = nullptr) : QThread(parent) {}
    ~ThumbnailWorker() override;
    virtual void request(const ThumbnailRequest &request);
    virtual void cancel();
signals:
    void ready(QImage image, ThumbnailRequest request);
protected:
    void run() override;
private:
    QMutex mutex;
    QWaitCondition pending;
    ThumbnailRequest pendingRequest;
    quint64 cancellation = 0;
    bool requested = false;
    bool stopping = false;
};

// Owns scheduling, stale-result rejection and a byte-budgeted, process-local cache.
class ThumbnailService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Thumbnails belong to the application")
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY changed)
    Q_PROPERTY(bool approximate READ approximate NOTIFY changed)
public:
    explicit ThumbnailService(QObject *parent = nullptr);
    ThumbnailService(ThumbnailWorker &worker, QObject *parent = nullptr);
    Q_INVOKABLE void request(const QString &path, double seconds);
    Q_INVOKABLE void clear();
    QString imageUrl() const;
    QImage currentImage() const { return image; }
    bool approximate() const { return imageApproximate; }
private:
    void initialize();
    void dispatch(bool precise);
    void acceptImage(const QImage &frame, const ThumbnailRequest &request);
    void displayImage(const QImage &frame, bool approximate);
    std::unique_ptr<ThumbnailWorker> ownedWorker;
    ThumbnailWorker &worker;
    QTimer throttle;
    QTimer refinement;
    QCache<QString, QImage> cache{32 * 1024 * 1024};
    ThumbnailRequest target;
    QImage image;
    bool active = false;
    bool imageApproximate = false;
    quint64 imageRevision = 0;
    QString lastDispatchedKey;
signals:
    void changed();
};
