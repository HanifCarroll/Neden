#pragma once
#include "cues.h"
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>
#include <functional>

class StudyTools : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Study services are supplied by the application")
    Q_PROPERTY(QString answer READ answer NOTIFY changed)
    Q_PROPERTY(QString subject READ subject NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString endpoint READ endpoint NOTIFY changed)
    Q_PROPERTY(QString model READ model NOTIFY changed)
    Q_PROPERTY(QString dictionaryTemplate READ dictionaryTemplate NOTIFY changed)
    Q_PROPERTY(QVariantList cards READ cards NOTIFY changed)
    Q_PROPERTY(QVariantList subtitleResults READ subtitleResults NOTIFY changed)
    Q_PROPERTY(bool automaticSubtitles READ automaticSubtitles NOTIFY changed)
public:
    using CredentialReader = std::function<QByteArray(const QString &)>;
    explicit StudyTools(QString storageDirectory, CredentialReader reader, QObject *parent = nullptr,
        QString subtitleApi = "https://api.opensubtitles.com/api/v1");
    ~StudyTools() override;
    QString answer() const { return answerText; }
    QString subject() const { return subjectText; }
    QString state() const { return requestState; }
    QString message() const { return statusText; }
    QString endpoint() const { return endpointText; }
    QString model() const { return modelText; }
    QString dictionaryTemplate() const { return dictionaryPattern; }
    QVariantList cards() const { return savedCards.toVariantList(); }
    QVariantList subtitleResults() const { return results; }
    bool automaticSubtitles() const { return automatic; }
    void setMedia(const QString &path, bool searchSubtitles = true);
    void setSubtitleLoginReader(std::function<QByteArray()> reader) { subtitleLoginReader = std::move(reader); }
    void setCredentialAuthorizer(std::function<QString(const QString &)> authorizer) { credentialAuthorizer = std::move(authorizer); }
    Q_INVOKABLE void connectCredential(bool subtitles = false);
    void invalidateContext();
    void invalidateTrack();
    Q_INVOKABLE bool configure(const QString &endpoint, const QString &model, const QString &dictionary, bool automatic);
    Q_INVOKABLE QUrl dictionaryUrl(const QString &term);
    Q_INVOKABLE void explain(const QString &selection, const QString &context);
    Q_INVOKABLE void retryExplanation();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void saveCard(const QString &front, const QString &context, const QString &translation, double seconds);
    Q_INVOKABLE void removeCard(int index);
    Q_INVOKABLE void reviewCard(int index, bool remembered);
    Q_INVOKABLE void exportCards(const QUrl &file);
    Q_INVOKABLE void findSubtitles(const QString &language = "tr", bool autoDownload = false, const QString &query = {}, int season = 0, int episode = 0);
    Q_INVOKABLE void downloadSubtitle(int index);
    static QString movieHash(const QString &path);
    static QUrl buildDictionaryUrl(const QString &pattern, const QString &term);
signals:
    void changed();
    void subtitlesReady(QVector<Cue> cues, QString savedPath);
private:
    bool persist();
    void consumeEvents();
    void fail(const QString &message);
    void cancelSubtitles();
    QNetworkRequest authenticatedRequest(const QUrl &url, const QString &credentialEndpoint, bool subtitles);
    void fetchSubtitle(const QUrl &url, quint64 owner);
    void requestSubtitleDownload(int index, quint64 owner);
    QString storage;
    QString subtitleEndpoint;
    CredentialReader credentials;
    std::function<QString(const QString &)> credentialAuthorizer;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> aiReply;
    QPointer<QNetworkReply> subtitleReply;
    std::function<QByteArray()> subtitleLoginReader;
    QByteArray subtitleToken;
    QByteArray pendingEvents;
    QString answerText;
    QString subjectText;
    QString subjectContext;
    QString requestState = "idle";
    QString statusText = "Select a subtitle to study.";
    QString endpointText = "https://api.deepseek.com";
    QString modelText = "deepseek-flash";
    QString dictionaryPattern = "https://en.wiktionary.org/wiki/%s";
    QString mediaPath;
    QJsonArray savedCards;
    QVariantList results;
    quint64 subtitleOwner = 0;
    bool automatic = false;
    bool sawDone = false;
    bool truncated = false;
    bool validStorage = true;
};
