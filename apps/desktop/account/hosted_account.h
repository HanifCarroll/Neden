#pragma once
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>

#ifndef NEDEN_CLOUD_API_ORIGIN
#define NEDEN_CLOUD_API_ORIGIN "https://neden-api-staging.hanifcarroll.com"
#endif
#ifndef NEDEN_CLOUD_WEBSITE_ORIGIN
#define NEDEN_CLOUD_WEBSITE_ORIGIN "https://neden-staging.hanifcarroll.com"
#endif

// Owns browser authorization, token renewal and authenticated hosted requests.
// Refresh tokens cross only the injected credential-store boundary, never QML or settings.
class HostedAccount : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The application supplies the hosted account")
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QVariantMap allowance READ allowance NOTIFY changed)
public:
    using ReadCompletion = std::function<void(QByteArray, QString)>;
    using WriteCompletion = std::function<void(QString)>;
    using Reader = std::function<void(ReadCompletion)>;
    using Writer = std::function<void(QByteArray, WriteCompletion)>;
    explicit HostedAccount(QObject *parent = nullptr,
                           QUrl api = QUrl(NEDEN_CLOUD_API_ORIGIN),
                           QUrl website = QUrl(NEDEN_CLOUD_WEBSITE_ORIGIN));
    ~HostedAccount() override;
    void setCredentialStore(Reader reader, Writer writer);
    void restoreToken(QByteArray token);
    QString credentialService() const { return "io.github.hanifcarroll.neden.cloud:" + apiOrigin.toString(); }
    bool enabled() const { return useHosted; }
    bool connected() const;
    bool busy() const { return signingIn || renewing; }
    QString message() const { return statusText; }
    QVariantMap allowance() const { return accountSummary; }
    void setEnabled(bool enabled);
    Q_INVOKABLE void signIn();
    Q_INVOKABLE void connectSaved();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void refreshAccount();
    Q_INVOKABLE void manageAccount();
    QNetworkReply *postStudy(const QJsonObject &body);
    static QJsonObject languageContext(const QJsonObject &context);
signals:
    void changed();
    void browserRequested(const QUrl &url);
private:
    void acceptCallback();
    void exchange(QUrlQuery parameters, bool activate = false);
    void renew(bool activate = false);
    void cancelSignIn();
    void setMessage(const QString &message);
    QNetworkReply *send(const QString &path, const QByteArray &body, bool form = false);
    QNetworkRequest authenticatedRequest(const QString &path) const;
    QUrl apiOrigin, websiteOrigin, redirectUri;
    QNetworkAccessManager network;
    QTcpServer callbackServer;
    QTimer signInTimeout, refreshTimer;
    QPointer<QNetworkReply> tokenReply;
    Reader readToken;
    Writer writeToken;
    QByteArray accessToken, refreshToken, verifier, expectedState;
    qint64 expiresAt = 0;
    quint64 generation = 0;
    bool useHosted = false, signingIn = false, renewing = false;
    QString statusText = "Sign in to the invited Neden Cloud preview, or use your own API key.";
    QVariantMap accountSummary;
};
