#pragma once
#include "ai_client.h"
#include "word_batch.h"
#include <QCache>
#include <QSet>
#include <QTimer>
#include <QNetworkReply>
#include <QPointer>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>
#include <QtQml/qqmlregistration.h>
#include <functional>

class WordLookup : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Word lookup is supplied by study services")
    Q_PROPERTY(QString detailsState READ detailsState NOTIFY changed)
    Q_PROPERTY(QString detailsMessage READ detailsMessage NOTIFY changed)
    Q_PROPERTY(QString word READ word NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QStringList meanings READ meanings NOTIFY changed)
    Q_PROPERTY(QString lemma READ lemma NOTIFY changed)
    Q_PROPERTY(QString partOfSpeech READ partOfSpeech NOTIFY changed)
    Q_PROPERTY(QString contextNote READ contextNote NOTIFY changed)
    Q_PROPERTY(QString formNote READ formNote NOTIFY changed)
    Q_PROPERTY(QVariantList otherSenses READ otherSenses NOTIFY changed)
    Q_PROPERTY(QVariantList otherUses READ otherUses NOTIFY changed)
public:
    using CredentialReader = std::function<QByteArray(const QString &)>;
    WordLookup(CredentialReader credentialReader, QObject *parent = nullptr);
    QString word() const { return selectedWord; }
    QString state() const { return requestState; }
    QString message() const { return statusText; }
    QStringList meanings() const { return definitions; }
    QString lemma() const { return baseWord; }
    QString partOfSpeech() const { return grammaticalCategory; }
    QString contextNote() const { return usageNote; }
    QString formNote() const { return wordFormNote; }
    QVariantList otherSenses() const { return alternativeSenses; }
    QVariantList otherUses() const { return relatedUses; }
    void lookupContext(const QString &word, const QJsonObject &context, const QString &endpoint,
                       const QString &model, bool noKeyRequired = false, int requestDelayMs = 0);
    void cancel();
    Q_INVOKABLE void loadDetails();
    QString detailsState() const { return detailState; }
    QString detailsMessage() const { return detailMessage; }
    static QString wordAt(const QString &text, int offset);
    Q_INVOKABLE QVariantMap wordRangeAt(const QString &text, int offset) const;
    static QVariantMap parseContextMeaning(const QByteArray &body);
    void setHostedAccount(HostedAccount *account);
signals:
    void changed();

private:
    struct CachedLine {
        QMap<QString, QVariantMap> cards;
        QSet<QString> detailedWords;
    };
    QString contextCacheKey(const QJsonObject &context, const QString &endpoint,
                            const QString &model, bool noKeyRequired) const;
    void observe(QNetworkReply *reply);
    void consumeBatch();
    void showSelectedCard();
    void abortDetails();
    void abortBatch();
    void clearCard();
    QCache<QString, CachedLine> cache{64};
    QString cacheKey, providerEndpoint, providerModel;
    QJsonObject selectedContext;
    bool providerNeedsNoKey = false;
    QString detailState = "idle", detailMessage;
    QTimer requestStartTimer{this};
    QPointer<QNetworkReply> batchReply;
    WordBatch batch;
    QString batchError;

    QMetaObject::Connection accountConnection;
    bool accountConnected = false;
    AiClient aiClient;
    QPointer<QNetworkReply> reply;
    QString selectedWord;
    QString requestState = "idle";
    QString statusText;
    QStringList definitions;
    QString baseWord;
    QString grammaticalCategory;
    QString usageNote;
    QString wordFormNote;
    QVariantList alternativeSenses;
    QVariantList relatedUses;
};
