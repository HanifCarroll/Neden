#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
class HostedAccount;
struct ChatRequest {
    QJsonObject hostedBody;
    QString endpoint, model;
    QJsonArray messages;
    bool streamResponse = true, requireJsonResponse = false, noKeyRequired = false;
    int maxTokens = 1600, timeoutMs = 60000;
    qint64 responseLimitBytes = 8 * 1024 * 1024;
};
// Feature code supplies messages; this client owns provider wire options and private authentication.
class AiClient {
public:
    using CredentialReader = std::function<QByteArray(const QString &)>;
    explicit AiClient(CredentialReader credentialReader) : readCredential(std::move(credentialReader)) {}
    QNetworkReply *post(const ChatRequest &request);
    void setHostedAccount(HostedAccount *account) { hosted = account; }
    bool usesHostedAccount() const;
    static bool allowedEndpoint(const QUrl &url);

private:
    HostedAccount *hosted = nullptr;
    CredentialReader readCredential;
    QNetworkAccessManager networkAccessManager;
};
