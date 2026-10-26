#include "ai_client.h"
#include "hosted_account.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslSocket>
#include <stdexcept>
bool AiClient::allowedEndpoint(const QUrl &url) {
    const bool isLoopbackEndpoint =
        url.scheme() == "http" &&
        (url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1");
    return url.isValid() && !url.host().isEmpty() && (url.scheme() == "https" || isLoopbackEndpoint) &&
           url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment();
}
bool AiClient::usesHostedAccount() const { return hosted && hosted->enabled(); }
QNetworkReply *AiClient::post(const ChatRequest &options) {
    if (usesHostedAccount()) {
        if (options.hostedBody.isEmpty()) throw std::runtime_error("Missing hosted study context.");
        return hosted->postStudy(options.hostedBody);
    }
    if (!allowedEndpoint(QUrl(options.endpoint)) || options.model.trimmed().isEmpty() ||
        options.model.size() > 200)
        throw std::runtime_error("Choose a valid AI endpoint and model in Settings.");
    if (QUrl(options.endpoint).scheme() == "https" && !QSslSocket::supportsSsl())
        throw std::runtime_error(
            "This installation is missing secure networking support. Reinstall Neden; no request was sent.");
    QNetworkRequest request(QUrl(options.endpoint + "/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(options.timeoutMs);
    if (!options.noKeyRequired) {
        auto key = readCredential(options.endpoint);
        if (key.isEmpty() || key.size() > 512 || key.contains('\r') || key.contains('\n'))
            throw std::runtime_error(
                "The saved credential is missing or Keychain access is unavailable. No request was sent.");
        request.setRawHeader("Authorization", "Bearer " + key);
        key.fill(0);
    }
    QJsonObject body{{"model", options.model},
                     {"stream", options.streamResponse},
                     {"max_tokens", options.maxTokens},
                     {"messages", options.messages}};
    if (options.requireJsonResponse)
        body.insert("response_format", QJsonObject{{"type", "json_object"}});
    if (QUrl(options.endpoint).host() == "api.deepseek.com")
        body.insert("thinking", QJsonObject{{"type", "disabled"}});
    if (options.model == "gpt-6-luna") {
        body.insert("reasoning_effort", "none");
        body.insert("store", false);
    }
    const auto bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (bytes.size() > 100000)
        throw std::runtime_error("The complete AI request exceeds its size limit. Nothing was sent.");
    auto *reply = networkAccessManager.post(request, bytes);
    reply->setReadBufferSize(options.responseLimitBytes + 1);
    return reply;
}
