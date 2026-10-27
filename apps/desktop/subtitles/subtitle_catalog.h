#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QVariantList>
#include <functional>

// Owns provider-specific search and download protocols. Results contain opaque
// provider IDs; callers never construct download URLs or handle API credentials.
class SubtitleCatalog : public QObject {
    Q_OBJECT
public:
    using CredentialReader = std::function<QByteArray(const QString &)>;
    explicit SubtitleCatalog(CredentialReader credentials, QObject *parent = nullptr,
        QString subvaultOrigin = "https://api.gestdown.info",
        QString subdlOrigin = "https://api.subdl.com",
        QString subdlDownloadOrigin = "https://dl.subdl.com");
    void search(const QString &provider, const QString &title, const QString &language, int season, int episode);
    void select(const QVariantMap &result);
    void cancel();
signals:
    void resultsReady(QVariantList results, QString message);
    void messageChanged(QString message);
    void fileReady(QByteArray bytes);
private:
    using ResponseHandler = std::function<void(QByteArray)>;
    void get(const QUrl &url, ResponseHandler completed, bool subdlCredential = false);
    void searchTitle(const QVariantMap &title);
    void publishSubtitles(const QByteArray &bytes);
    QUrl downloadUrl(const QString &path) const;
    CredentialReader credentials;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
    QString vaultOrigin, dlOrigin, downloadOrigin;
    QString provider, language;
    int season = 0, episode = 0;
    quint64 generation = 0;
};
