#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QVariantMap>
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
public:
    using CredentialReader = std::function<QByteArray(const QString &)>;
    WordLookup(CredentialReader reader, QObject *parent = nullptr, QUrl testOrigin = {});
    QString word() const { return selectedWord; }
    QString state() const { return requestState; }
    QString message() const { return statusText; }
    QStringList meanings() const { return definitions; }
    void lookup(const QString &word, const QVariantMap &settings, bool translationOnly = false);
    void cancel();
    static QString wordAt(const QString &text, int offset);
    static QString languageCode(const QString &language);
    static QStringList parseMeanings(const QByteArray &body, bool google, bool dictionary);
signals:
    void changed();
private:
    CredentialReader credentials;
    QUrl testOrigin;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
    QString selectedWord;
    QString requestState = "idle";
    QString statusText;
    QStringList definitions;
};
