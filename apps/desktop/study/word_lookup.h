#pragma once
#include "ai_client.h"
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
                       const QString &model, bool noKeyRequired = false);
    void cancel();
    static QString wordAt(const QString &text, int offset);
    Q_INVOKABLE QVariantMap wordRangeAt(const QString &text, int offset) const;
    static QVariantMap parseContextMeaning(const QByteArray &body);
signals:
    void changed();

private:
    void observe(QNetworkReply *reply);
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
