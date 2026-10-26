#include "hosted_account.h"
#include "hosted_context.h"
#include "ai_client.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QUuid>
#include <stdexcept>
#include <memory>

namespace {
QByteArray randomUrlToken() {
    QByteArray bytes(32, Qt::Uninitialized);
    for (auto &byte : bytes)
        byte = static_cast<char>(QRandomGenerator::system()->generate() & 0xff);
    return bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}
bool validToken(const QByteArray &value) {
    return !value.isEmpty() && value.size() <= 16000 && !value.contains('\r') && !value.contains('\n');
}
}
HostedAccount::HostedAccount(QObject *parent, QUrl api, QUrl website)
    : QObject(parent), apiOrigin(std::move(api)), websiteOrigin(std::move(website)) {
    if (!AiClient::allowedEndpoint(apiOrigin) || !AiClient::allowedEndpoint(websiteOrigin))
        throw std::invalid_argument("Invalid hosted account origin");
    connect(&callbackServer, &QTcpServer::newConnection, this, &HostedAccount::acceptCallback);
    signInTimeout.setSingleShot(true);
    signInTimeout.setInterval(180000);
    connect(&signInTimeout, &QTimer::timeout, this, [this] {
        cancelSignIn();
        setMessage("Sign-in timed out. Choose Sign in to try again.");
    });
    refreshTimer.setSingleShot(true);
    connect(&refreshTimer, &QTimer::timeout, this, [this] { renew(); });
}
HostedAccount::~HostedAccount() {
    cancelSignIn();
    accessToken.fill(0);
    refreshToken.fill(0);
}
void HostedAccount::setCredentialStore(Reader reader, Writer writer) {
    readToken = std::move(reader);
    writeToken = std::move(writer);
}
bool HostedAccount::connected() const {
    return !accessToken.isEmpty() && expiresAt > QDateTime::currentSecsSinceEpoch() + 15;
}
void HostedAccount::setMessage(const QString &message) {
    statusText = message;
    emit changed();
}
void HostedAccount::setEnabled(bool enabled) {
    if (useHosted == enabled) return;
    useHosted = enabled;
    emit changed();
}
void HostedAccount::cancelSignIn() {
    signInTimeout.stop();
    callbackServer.close();
    verifier.fill(0);
    verifier.clear();
    expectedState.clear();
    signingIn = false;
}
void HostedAccount::signIn() {
    if (busy()) return;
    if (!writeToken) {
        setMessage("The system credential store is unavailable.");
        return;
    }
    if (!callbackServer.listen(QHostAddress::LocalHost, 0)) {
        setMessage("Could not start the local sign-in callback. Try again.");
        return;
    }
    ++generation;
    signingIn = true;
    verifier = randomUrlToken();
    expectedState = randomUrlToken();
    redirectUri = QUrl(QString("http://127.0.0.1:%1/oauth/callback").arg(callbackServer.serverPort()));
    QUrl url(apiOrigin.toString() + "/api/auth/oauth2/authorize");
    QUrlQuery query;
    query.addQueryItem("client_id", "neden-desktop");
    query.addQueryItem("response_type", "code");
    query.addQueryItem("redirect_uri", redirectUri.toString());
    query.addQueryItem("scope", "openid profile email offline_access study");
    query.addQueryItem("resource", apiOrigin.toString());
    query.addQueryItem("state", QString::fromLatin1(expectedState));
    query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("code_challenge", QString::fromLatin1(QCryptographicHash::hash(verifier, QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
    url.setQuery(query);
    signInTimeout.start();
    setMessage("Finish signing in in your browser.");
    emit browserRequested(url);
}
void HostedAccount::acceptCallback() {
    while (auto *socket = callbackServer.nextPendingConnection()) {
        socket->setParent(this);
        socket->setReadBufferSize(8193);
        auto buffer = std::make_shared<QByteArray>();
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(5000, socket, [socket] { socket->disconnectFromHost(); });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
            *buffer += socket->readAll();
            if (buffer->size() > 8192) { socket->abort(); return; }
            if (!buffer->contains("\r\n\r\n")) return;
            const auto firstLine = buffer->left(buffer->indexOf("\r\n")).split(' ');
            const QUrl target(firstLine.size() == 3 ? QString::fromUtf8(firstLine.at(1)) : QString());
            const QUrlQuery query(target);
            const bool accepted = signingIn && firstLine.value(0) == "GET" && target.isRelative() &&
                target.path() == "/oauth/callback" && !expectedState.isEmpty() &&
                query.queryItemValue("state").toLatin1() == expectedState &&
                (!query.hasQueryItem("iss") || query.queryItemValue("iss", QUrl::FullyDecoded) == apiOrigin.toString() + "/api/auth");
            const QByteArray message = accepted ? "Sign-in received. You can return to Neden." : "Invalid sign-in callback.";
            socket->write(QByteArray("HTTP/1.1 ") + (accepted ? "200 OK" : "400 Bad Request") +
                "\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
                QByteArray::number(message.size()) + "\r\n\r\n" + message);
            socket->disconnectFromHost();
            buffer->clear();
            if (!accepted) return;
            const auto code = query.queryItemValue("code");
            QUrlQuery parameters;
            parameters.addQueryItem("grant_type", "authorization_code");
            parameters.addQueryItem("code", code);
            parameters.addQueryItem("code_verifier", QString::fromLatin1(verifier));
            parameters.addQueryItem("redirect_uri", redirectUri.toString());
            cancelSignIn();
            if (code.isEmpty() || code.size() > 4096) {
                setMessage("Sign-in was not completed. Choose Sign in to try again.");
                return;
            }
            exchange(parameters, true);
        });
    }
}
QNetworkReply *HostedAccount::send(const QString &path, const QByteArray &body, bool form) {
    QNetworkRequest request(QUrl(apiOrigin.toString() + path));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, form ? "application/x-www-form-urlencoded" : "application/json");
    auto *reply = network.post(request, body);
    reply->setReadBufferSize(64001);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 64000) reply->abort();
    });
    return reply;
}
void HostedAccount::exchange(QUrlQuery parameters, bool activate) {
    renewing = true;
    const auto requestGeneration = generation;
    parameters.addQueryItem("client_id", "neden-desktop");
    parameters.addQueryItem("resource", apiOrigin.toString());
    auto *reply = send("/api/auth/oauth2/token", parameters.query(QUrl::FullyEncoded).toUtf8(), true);
    tokenReply = reply;
    emit changed();
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestGeneration, activate] {
        reply->deleteLater();
        if (generation != requestGeneration) return;
        tokenReply = nullptr;
        renewing = false;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto data = QJsonDocument::fromJson(reply->readAll()).object();
        const auto access = data.value("access_token").toString().toUtf8();
        const auto refresh = data.value("refresh_token").toString().toUtf8();
        const int lifetime = data.value("expires_in").toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200 || !validToken(access) ||
            !validToken(refresh) || lifetime < 60 || lifetime > 86400) {
            if (status == 400 || status == 401) {
                accessToken.clear(); refreshToken.clear(); expiresAt = 0;
                setMessage("Your session expired. Sign in again.");
            } else {
                if (!refreshToken.isEmpty()) refreshTimer.start(30000);
                setMessage("Could not reconnect to Neden Cloud. Check your connection or try Sign in again.");
            }
            return;
        }
        accessToken = access;
        refreshToken = refresh;
        expiresAt = QDateTime::currentSecsSinceEpoch() + lifetime;
        refreshTimer.start((lifetime - 60) * 1000);
        if (activate) setEnabled(true);
        setMessage("Connected to the invited Neden Cloud preview.");
        writeToken(refreshToken, [this, requestGeneration](QString error) {
            if (generation != requestGeneration) return;
            if (!error.isEmpty()) setMessage("Connected for this session; the system credential store could not save sign-in.");
        });
        refreshAccount();
    });
}
void HostedAccount::connectSaved() {
    if (busy() || !readToken) return;
    const auto requestGeneration = ++generation;
    renewing = true;
    setMessage("Connecting saved account…");
    readToken([this, requestGeneration](QByteArray token, QString error) {
        if (requestGeneration != generation) return;
        renewing = false;
        if (!error.isEmpty() || !validToken(token)) {
            setMessage("Choose Sign in to connect your Neden account.");
            return;
        }
        refreshToken = token;
        renew(true);
    });
}
void HostedAccount::restoreToken(QByteArray token) {
    if (busy() || !validToken(token)) return;
    refreshToken = token;
    renew();
}
void HostedAccount::renew(bool activate) {
    if (busy() || refreshToken.isEmpty()) return;
    QUrlQuery parameters;
    parameters.addQueryItem("grant_type", "refresh_token");
    parameters.addQueryItem("refresh_token", QString::fromLatin1(refreshToken));
    exchange(parameters, activate);
}
void HostedAccount::signOut() {
    ++generation;
    cancelSignIn();
    refreshTimer.stop();
    if (tokenReply) { tokenReply->abort(); tokenReply->deleteLater(); tokenReply = nullptr; }
    renewing = false;
    if (!refreshToken.isEmpty()) {
        QUrlQuery parameters;
        parameters.addQueryItem("client_id", "neden-desktop");
        parameters.addQueryItem("token", QString::fromLatin1(refreshToken));
        parameters.addQueryItem("token_type_hint", "refresh_token");
        auto *reply = send("/api/auth/oauth2/revoke", parameters.query(QUrl::FullyEncoded).toUtf8(), true);
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
    accessToken.fill(0); accessToken.clear();
    refreshToken.fill(0); refreshToken.clear();
    expiresAt = 0;
    accountSummary.clear();
    setEnabled(false);
    setMessage("Signed out of Neden Cloud.");
    if (writeToken) writeToken({}, [this](QString error) {
        if (!error.isEmpty()) setMessage("Signed out here, but saved sign-in could not be removed from the system credential store.");
    });
}
QNetworkRequest HostedAccount::authenticatedRequest(const QString &path) const {
    QNetworkRequest request(QUrl(apiOrigin.toString() + path));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(90000);
    request.setRawHeader("Authorization", "Bearer " + accessToken);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return request;
}
void HostedAccount::refreshAccount() {
    if (!connected()) return;
    const auto requestGeneration = generation;
    auto *reply = network.get(authenticatedRequest("/v1/account"));
    reply->setReadBufferSize(16001);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 16000) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestGeneration] {
        reply->deleteLater();
        if (generation != requestGeneration) return;
        if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            accountSummary = QJsonDocument::fromJson(reply->readAll()).object().toVariantMap();
            emit changed();
        }
    });
}
void HostedAccount::manageAccount() {
    emit browserRequested(QUrl(websiteOrigin.toString() + "/account/"));
}
QJsonObject HostedAccount::languageContext(const QJsonObject &context) {
    return hostedLanguageContext(context);
}
QNetworkReply *HostedAccount::postStudy(const QJsonObject &body) {
    if (!connected()) {
        renew();
        throw std::runtime_error("Neden Cloud is reconnecting. Try again shortly, or connect your account in Settings → Services.");
    }
    const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (bytes.size() > 32000)
        throw std::runtime_error("This conversation is too long. Start a new explanation.");
    auto request = authenticatedRequest("/v1/study");
    request.setRawHeader("Idempotency-Key", QUuid::createUuid().toByteArray(QUuid::WithoutBraces));
    auto *reply = network.post(request, bytes);
    reply->setProperty("nedenHosted", true);
    reply->setReadBufferSize(256001);
    connect(reply, &QNetworkReply::finished, this, &HostedAccount::refreshAccount);
    return reply;
}
