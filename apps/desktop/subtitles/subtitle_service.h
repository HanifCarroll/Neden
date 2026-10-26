#pragma once
#include "cues.h"
#include "app_settings.h"
#include "ai_client.h"
#include <QPointer>
class SubtitleService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Subtitle acquisition is supplied by the application")
    Q_PROPERTY(QVariantList subtitleResults READ subtitleResults NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    SubtitleService(QString directory, AppSettings &settings, AiClient::CredentialReader credentialReader,
                    QObject *parent = nullptr, QString endpoint = "https://api.opensubtitles.com/api/v1");
    ~SubtitleService() override { cancel(); }
    QVariantList subtitleResults() const { return results; }
    QString message() const { return statusText; }
    void setMedia(const QString &path, bool search = true);
    void setSubtitleLoginReader(std::function<QByteArray()> credentialReader) {
        subtitleLoginReader = std::move(credentialReader);
    }
    void cancel();
    Q_INVOKABLE void findSubtitles(const QString &language, bool autoDownload = false,
                                   const QString &query = {}, int season = 0, int episode = 0);
    Q_INVOKABLE void downloadSubtitle(int index);
    static QString movieHash(const QString &path);
signals:
    void changed();
    void subtitlesReady(QVector<Cue> cues, QString savedPath);

private:
    void setMessage(const QString &message) {
        statusText = message;
        emit changed();
    }
    QNetworkRequest authenticatedRequest(const QUrl &url);
    void fetchSubtitle(const QUrl &url, quint64 requestGenerationAtStart);
    void requestSubtitleDownload(int index, quint64 requestGenerationAtStart);
    QString storage, subtitleEndpoint, mediaPath, statusText;
    AppSettings &settings;
    AiClient::CredentialReader readCredential;
    QNetworkAccessManager networkAccessManager;
    QPointer<QNetworkReply> subtitleReply;
    std::function<QByteArray()> subtitleLoginReader;
    QByteArray subtitleToken;
    QVariantList results;
    quint64 requestGeneration = 0;
};
