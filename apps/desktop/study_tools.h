#pragma once
#include "cues.h"
#include "conversation.h"
#include "word_lookup.h"
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
    Q_PROPERTY(WordLookup *words READ words CONSTANT)
    Q_PROPERTY(QString answer READ answer NOTIFY changed)
    Q_PROPERTY(QString subject READ subject NOTIFY changed)
    Q_PROPERTY(QVariantList turns READ turns NOTIFY changed)
    Q_PROPERTY(QString sourceCue READ sourceCue NOTIFY changed)
    Q_PROPERTY(QString sourceLanguage READ sourceLanguage NOTIFY changed)
    Q_PROPERTY(QString explanationLanguage READ explanationLanguage NOTIFY changed)
    Q_PROPERTY(bool includeSecondary READ includeSecondary NOTIFY changed)
    Q_PROPERTY(QVariantMap learningSettings READ learningSettings NOTIFY changed)
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
        QString subtitleApi = "https://api.opensubtitles.com/api/v1",
        QString alternateApi = "https://api.openai.com/v1");
    ~StudyTools() override;
    WordLookup *words() { return &wordLookup; }
    QString answer() const { return answerText; }
    QString subject() const { return subjectText; }
    QVariantList turns() const { return conversation.turns(); }
    QString sourceCue() const { return subjectContext; }
    QString sourceLanguage() const { return learningPreferences.value("sourceLanguage").toString("Turkish"); }
    QString explanationLanguage() const { return learningPreferences.value("explanationLanguage").toString("English"); }
    bool includeSecondary() const { return learningPreferences.value("includeSecondary").toBool(true); }
    QVariantMap learningSettings() const { return learningPreferences.toVariantMap(); }
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
    void setCredentialWriter(std::function<QString(const QString &, QByteArray)> writer) { credentialWriter = std::move(writer); }
    Q_INVOKABLE void saveServiceKey(const QString &service, const QString &key);
    Q_INVOKABLE void connectServiceKey(const QString &service);
    Q_INVOKABLE bool configureLearning(const QVariantMap &preferences);
    Q_INVOKABLE void invalidateContext();
    void invalidateTrack();
    Q_INVOKABLE bool configure(const QString &endpoint, const QString &model, const QString &dictionary, bool automatic);
    Q_INVOKABLE QUrl dictionaryUrl(const QString &term);
    Q_INVOKABLE void explain(const QString &selection, const QString &context);
    void explainContext(const QJsonObject &context);
    Q_INVOKABLE bool followUp(const QString &question);
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
    void launchConversation(bool fallback = false);
    QString credentialService(const QString &service) const;
    void consumeEvents();
    void fail(const QString &message);
    void cancelSubtitles();
    QNetworkRequest authenticatedRequest(const QUrl &url, const QString &credentialEndpoint, bool subtitles);
    void fetchSubtitle(const QUrl &url, quint64 owner);
    void requestSubtitleDownload(int index, quint64 owner);
    QString storage;
    QString subtitleEndpoint;
    QString alternateEndpoint;
    CredentialReader credentials;
    std::function<QString(const QString &)> credentialAuthorizer;
    std::function<QString(const QString &, QByteArray)> credentialWriter;
    WordLookup wordLookup;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> aiReply;
    QPointer<QNetworkReply> subtitleReply;
    std::function<QByteArray()> subtitleLoginReader;
    QByteArray subtitleToken;
    QByteArray pendingEvents;
    QString answerText;
    QString subjectText;
    QString subjectContext;
    Conversation conversation;
    QJsonObject learningPreferences;
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
    bool fallbackWaiting = false;
    quint64 aiOwner = 0;
    bool sawDone = false;
    bool truncated = false;
    bool validStorage = true;
};
