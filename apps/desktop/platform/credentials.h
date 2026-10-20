#pragma once
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

// Privileged service only: secret bytes never become QML properties or settings.
class CredentialStore : public QObject {
    Q_OBJECT
public:
    using Completion = std::function<void(QString)>;
    explicit CredentialStore(QObject *parent = nullptr) : QObject(parent) {}
    QByteArray read(const QString &endpoint);
    QByteArray subtitleLogin() const;
    void authorize(const QString &endpoint, Completion completed);
    void save(const QString &endpoint, QByteArray key, Completion completed);
    static QString serviceName(const QString &endpoint);
    static bool validSecret(const QByteArray &secret, const QString &account);
private:
    void readAccounts(const QString &endpoint, const QStringList &accounts, int index,
                      QHash<QString, QByteArray> values, Completion completed);
    QHash<QString, QByteArray> authorizedKeys;
    bool operationPending = false;
};

// macOS compatibility path: background requests may read already-approved items,
// but only an explicit Connect/Save action can invoke QtKeychain and display a prompt.
QByteArray readCredentialWithoutInteraction(const QString &service, const QString &account);
