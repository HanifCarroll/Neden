#include "hosted_account.h"
#include "ai_client.h"
#include "conversation.h"
#include <QTest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QFile>
#include <memory>

class HostedAccountTests : public QObject {
    Q_OBJECT
private slots:
    void authorizationAndRequestIsolation() {
        QTcpServer api;
        QVERIFY(api.listen(QHostAddress::LocalHost, 0));
        const QUrl origin(QString("http://127.0.0.1:%1").arg(api.serverPort()));
        HostedAccount account(nullptr, origin, origin);
        QByteArray saved;
        account.setCredentialStore([&](auto done) { done(saved, {}); },
                                   [&](QByteArray token, auto done) { saved = token; done({}); });
        QByteArray exchangedVerifier, studyBody, studyAuthorization;
        int tokenRequests = 0;
        QObject::connect(&api, &QTcpServer::newConnection, &api, [&] {
            auto *socket = api.nextPendingConnection();
            auto received = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, received] {
                *received += socket->readAll();
                int split = received->indexOf("\r\n\r\n");
                if (split < 0) return;
                int length = 0;
                const auto headers = received->left(split).split('\n');
                for (const auto &line : headers)
                    if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                if (received->size() < split + 4 + length) return;
                const auto path = headers.first().split(' ').value(1);
                const auto body = received->mid(split + 4, length);
                QByteArray response = "{}";
                if (path == "/api/auth/oauth2/token") {
                    ++tokenRequests;
                    const QUrlQuery parameters(QString::fromUtf8(body));
                    if (parameters.queryItemValue("grant_type") == "authorization_code")
                        exchangedVerifier = parameters.queryItemValue("code_verifier").toUtf8();
                    else QCOMPARE(parameters.queryItemValue("refresh_token"), QString("refresh-test"));
                    response = R"({"access_token":"access-test","refresh_token":"refresh-test","expires_in":600,"token_type":"Bearer"})";
                } else if (path == "/v1/study") {
                    studyBody = body;
                    for (const auto &line : headers)
                        if (line.toLower().startsWith("authorization:")) studyAuthorization = line.mid(14).trimmed();
                    response = R"({"choices":[]})";
                }
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(response.size()) + "\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
        });
        QSignalSpy browser(&account, &HostedAccount::browserRequested);
        account.signIn();
        QCOMPARE(browser.size(), 1);
        const QUrlQuery authorization(browser.first().first().toUrl());
        QCOMPARE(authorization.queryItemValue("code_challenge_method"), QString("S256"));
        QVERIFY(authorization.queryItemValue("state").size() >= 40);
        QUrl callback(authorization.queryItemValue("redirect_uri"));
        QNetworkAccessManager client;
        const auto attempt = [&](QString state) {
            QUrlQuery query;
            query.addQueryItem("state", state);
            query.addQueryItem("code", "code-test");
            query.addQueryItem("iss", QString::fromLatin1(QUrl::toPercentEncoding(origin.toString() + "/api/auth")));
            callback.setQuery(query);
            return client.get(QNetworkRequest(callback));
        };
        auto *invalid = attempt("incorrect-state");
        QSignalSpy rejected(invalid, &QNetworkReply::finished);
        QVERIFY(rejected.wait());
        QCOMPARE(invalid->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 400);
        QCOMPARE(tokenRequests, 0);
        QVERIFY(account.busy());
        auto *valid = attempt(authorization.queryItemValue("state"));
        QSignalSpy accepted(valid, &QNetworkReply::finished);
        QVERIFY(accepted.wait());
        QTRY_VERIFY(account.connected());
        QCOMPARE(saved, QByteArray("refresh-test"));
        QCOMPARE(QCryptographicHash::hash(exchangedVerifier, QCryptographicHash::Sha256)
                     .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals),
                 authorization.queryItemValue("code_challenge").toUtf8());
        AiClient ai([](const QString &) { return QByteArray("private-byok-key"); });
        ai.setHostedAccount(&account);
        ChatRequest request;
        request.endpoint = "https://never-contact.example.com";
        request.hostedBody = {{"operation", "word"}, {"word", "test"}};
        auto *reply = ai.post(request);
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QVERIFY(finished.wait());
        QCOMPARE(studyAuthorization, QByteArray("Bearer access-test"));
        QVERIFY(!studyBody.contains("private-byok-key"));
        account.connectSaved();
        QTRY_COMPARE(tokenRequests, 2);
        QTRY_VERIFY(!account.busy());
        account.signOut();
        QVERIFY(!account.connected());
        QVERIFY(!account.enabled());
        QVERIFY(saved.isEmpty());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, account.postStudy(request.hostedBody));
    }
    void onlyLanguageEvidenceLeavesDesktop() {
        const QJsonObject context{{"selection", "hello"}, {"sourceCue", "hello there"},
                                  {"mediaPath", "/private/file.mkv"}, {"sourceLanguage", "English"},
                                  {"explanationLanguage", "Turkish"}};
        QVERIFY(!HostedAccount::languageContext(context).contains("mediaPath"));
        Conversation conversation;
        conversation.begin(context);
        QVERIFY(!conversation.hostedRequest().value("context").toObject().contains("mediaPath"));
        conversation.append("first answer");
        conversation.finish("complete");
        conversation.followUp("why?");
        const auto request = conversation.hostedRequest();
        QCOMPARE(request.value("operation").toString(), QString("followup"));
        QCOMPARE(request.value("history").toArray().size(), 1);
        QCOMPARE(request.value("question").toString(), QString("why?"));
    }
};
QTEST_GUILESS_MAIN(HostedAccountTests)
#include "hosted_account_tests.moc"
