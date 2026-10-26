#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// A manual, anonymous stable-release check. Never downloads or installs executable code.
class ReleaseCheck : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QUrl releaseUrl READ releaseUrl NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
public:
    explicit ReleaseCheck(QObject *parent = nullptr);
    bool checking() const { return inProgress; }
    QString message() const { return statusMessage; }
    QUrl releaseUrl() const { return availableRelease; }
    QString currentVersion() const;
    Q_INVOKABLE void check();
    // Shared with tests; only versioned links on our own repository are accepted.
    static QUrl newerRelease(const QJsonObject &release, const QString &installedVersion);
signals:
    void changed();
private:
    QNetworkAccessManager network;
    bool inProgress = false;
    QString statusMessage;
    QUrl availableRelease;
};
