#pragma once
#include "profile_store.h"
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <functional>
struct ConnectionSettings {
    QString endpoint = "https://api.deepseek.com", model = "deepseek-flash",
            dictionary = "https://en.wiktionary.org/wiki/%s";
    bool automaticSubtitles = false;
};
struct LearningSettings {
    QString sourceLanguage = "Turkish", explanationLanguage = "English", wordRegion;
    enum class WordProvider { ContextualAi, Off, Google, Microsoft };
    enum class PrimaryProvider { Configured, Luna };
    WordProvider wordProvider = WordProvider::ContextualAi;
    PrimaryProvider primaryProvider = PrimaryProvider::Configured;
    bool includeSecondary = true, noKeyRequired = false, fallbackEnabled = false;
    QJsonObject json() const;
    static LearningSettings parse(const QVariantMap &values);
};
class AppSettings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Settings are supplied by the application")
    Q_PROPERTY(QString endpoint READ endpoint NOTIFY changed)
    Q_PROPERTY(QString model READ model NOTIFY changed)
    Q_PROPERTY(QString dictionaryTemplate READ dictionaryTemplate NOTIFY changed)
    Q_PROPERTY(bool automaticSubtitles READ automaticSubtitles NOTIFY changed)
    Q_PROPERTY(QString sourceLanguage READ sourceLanguage NOTIFY changed)
    Q_PROPERTY(QString explanationLanguage READ explanationLanguage NOTIFY changed)
    Q_PROPERTY(bool includeSecondary READ includeSecondary NOTIFY changed)
    Q_PROPERTY(QVariantMap learningSettings READ learningSettings NOTIFY changed)
    Q_PROPERTY(QVariantList wordProviders READ wordProviders CONSTANT)
    Q_PROPERTY(QVariantList primaryProviders READ primaryProviders CONSTANT)
    Q_PROPERTY(QVariantList credentialServices READ credentialServices CONSTANT)
    Q_PROPERTY(QString wordProvider READ wordProvider NOTIFY changed)
    Q_PROPERTY(QString primaryProvider READ primaryProvider NOTIFY changed)
    Q_PROPERTY(bool noKeyRequired READ noKeyRequired NOTIFY changed)
    Q_PROPERTY(bool fallbackEnabled READ fallbackEnabled NOTIFY changed)
    Q_PROPERTY(QString wordRegion READ wordRegion NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
public:
    explicit AppSettings(ProfileStore &store, QObject *parent = nullptr);
    const ConnectionSettings &connection() const { return connectionValues; }
    const LearningSettings &learning() const { return learningValues; }
    QString endpoint() const { return connectionValues.endpoint; }
    QString model() const { return connectionValues.model; }
    QString dictionaryTemplate() const { return connectionValues.dictionary; }
    bool automaticSubtitles() const { return connectionValues.automaticSubtitles; }
    QString sourceLanguage() const { return learningValues.sourceLanguage; }
    QString explanationLanguage() const { return learningValues.explanationLanguage; }
    bool includeSecondary() const { return learningValues.includeSecondary; }
    QVariantMap learningSettings() const { return learningValues.json().toVariantMap(); }
    QJsonObject learningJson() const { return learningValues.json(); }
    QString wordProvider() const { return learningValues.json().value("wordProvider").toString(); }
    QString primaryProvider() const { return learningValues.json().value("primaryProvider").toString(); }
    bool noKeyRequired() const { return learningValues.noKeyRequired; }
    bool fallbackEnabled() const { return learningValues.fallbackEnabled; }
    QString wordRegion() const { return learningValues.wordRegion; }
    QString message() const { return statusText; }
    QVariantList wordProviders() const;
    QVariantList primaryProviders() const;
    QVariantList credentialServices() const;
    // Qt form data is a draft; all fields validate and persist together before state changes.
    Q_INVOKABLE bool applyDraft(const QVariantMap &draft);
    bool configure(const QString &endpoint, const QString &model, const QString &dictionary, bool automatic);
    bool configureLearning(const QVariantMap &values);
    Q_INVOKABLE QUrl dictionaryUrl(const QString &term);
    static QUrl buildDictionaryUrl(const QString &pattern, const QString &term);
    void setCredentialAuthorizer(std::function<QString(const QString &)> fn) { authorizer = std::move(fn); }
    void setCredentialWriter(std::function<QString(const QString &, QByteArray)> fn) {
        writer = std::move(fn);
    }
    Q_INVOKABLE void saveServiceKey(const QString &service, const QString &key);
    Q_INVOKABLE void connectServiceKey(const QString &service);
signals:
    void changed();
    void messageChanged();

private:
    bool commit(const ConnectionSettings &connection, const LearningSettings &learning);
    void setMessage(const QString &message) {
        statusText = message;
        emit messageChanged();
    }
    QString credentialService(const QString &service) const;
    ProfileStore &store;
    ConnectionSettings connectionValues;
    LearningSettings learningValues;
    QString statusText;
    std::function<QString(const QString &)> authorizer;
    std::function<QString(const QString &, QByteArray)> writer;
};
