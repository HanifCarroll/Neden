#include "study_services.h"
#include "subtitle_appearance.h"
#include "network_error.h"
#include <QTcpServer>
#include <QUrlQuery>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QJsonDocument>
#include <memory>
#include <QtTest>

class StudyTests : public QObject {
    Q_OBJECT
private slots:
    void settingsDraftCommitsAllFieldsOnlyAfterSuccessfulPersistence() {
        QTemporaryDir directory;
        StudyServices services(directory.path(),[](const QString &) { return QByteArray(); });
        auto *settings=services.settings();
        const auto originalEndpoint=settings->endpoint();
        auto draft=settings->learningSettings();
        draft.insert("endpoint","https://example.org/v1"); draft.insert("model","new-model");
        draft.insert("dictionary","https://example.org/dictionary/%s"); draft.insert("automaticSubtitles",true);
        draft.insert("wordProvider","unknown");
        QSignalSpy changes(settings,&AppSettings::changed);
        QVERIFY(!settings->applyDraft(draft));
        QCOMPARE(settings->endpoint(),originalEndpoint);
        QCOMPARE(changes.count(),0);
        draft.insert("wordProvider","ai");
        QVERIFY(settings->applyDraft(draft));
        QCOMPARE(settings->endpoint(),QString("https://example.org/v1"));
        QCOMPARE(settings->model(),QString("new-model"));
        QCOMPARE(changes.count(),1);
        const auto savedValues=settings->learningSettings();
        QVERIFY(QFile::rename(directory.filePath("study.json"),directory.filePath("preserved.json")));
        QVERIFY(QDir().mkdir(directory.filePath("study.json"))); // Force atomic replacement to fail.
        draft.insert("endpoint","https://different.example/v1"); draft.insert("sourceLanguage","Japanese");
        QVERIFY(!settings->applyDraft(draft));
        QCOMPARE(settings->endpoint(),QString("https://example.org/v1"));
        QCOMPARE(settings->learningSettings(),savedValues);
        QCOMPARE(changes.count(),1);
    }

    void featureStatusMessagesDoNotOverwriteAnExplanation() {
        QTemporaryDir directory;
        StudyServices services(directory.path(),[](const QString &) { return QByteArray(); });
        services.explanation()->explain("ışık","Bir ışık gördüm.");
        QCOMPARE(services.explanation()->state(),QString("failed"));
        const auto error=services.explanation()->message();
        services.deck()->saveCard("ışık","Bir ışık gördüm.","light",1.5);
        QVERIFY(services.deck()->message().contains("saved"));
        QCOMPARE(services.explanation()->message(),error);
        services.subtitles()->findSubtitles("invalid");
        QCOMPARE(services.explanation()->message(),error);
        QVERIFY(!services.subtitles()->message().isEmpty());
        StudyServices restored(directory.path(),[](const QString &) { return QByteArray(); });
        QCOMPARE(restored.deck()->cards().size(),1);
    }

    void contextualWordCardsRequireCompleteBoundedStructuredAnswers() {
        const auto envelope = [](const QJsonObject &card, const QString &finish = "stop") {
            return QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"finish_reason", finish},
                {"message", QJsonObject{{"content", QString::fromUtf8(QJsonDocument(card).toJson())}}}}}}}).toJson();
        };
        QJsonObject card{{"lemma", "dalmak"}, {"meaning", "I became absorbed"},
            {"explanation", "The speaker was absorbed in watching television."},
            {"otherMeanings", QJsonArray{"to dive into water", "to fall asleep"}}};
        const auto parsed = WordLookup::parseContextMeaning(envelope(card));
        QCOMPARE(parsed.value("lemma").toString(), QString("dalmak"));
        QCOMPARE(parsed.value("meaning").toString(), QString("I became absorbed"));
        QCOMPARE(parsed.value("otherMeanings").toStringList().size(), 2);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card, "length")));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning("{}"));
        card.insert("otherMeanings", QJsonArray{});
        QVERIFY(WordLookup::parseContextMeaning(envelope(card)).value("otherMeanings").toStringList().isEmpty());
        card.insert("meaning", QString(601, 'x'));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.insert("meaning", "<img src='https://invalid.example/tracker'>");
        // Returned text is never interpreted as HTML by the card.
        QVERIFY(WordLookup::parseContextMeaning(envelope(card)).value("meaning").toString().startsWith("<img"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(QByteArray(64001, 'x')));
    }

    void contextualWordRequestsUseFrozenContextAndCancelWithoutRetry() {
        QTemporaryDir directory;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        QJsonObject submitted;
        QByteArray headers;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            auto bytes = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, bytes] {
                *bytes += socket->readAll();
                const int split = bytes->indexOf("\r\n\r\n");
                if (split < 0 || socket->property("sent").toBool()) return;
                int length = 0;
                for (const auto &line : bytes->left(split).split('\n'))
                    if (line.toLower().startsWith("content-length:")) length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                if (bytes->size() < split + 4 + length) return;
                socket->setProperty("sent", true);
                ++requests;
                headers = bytes->left(split);
                submitted = QJsonDocument::fromJson(bytes->mid(split + 4, length)).object();
                const QString content = R"({"lemma":"dalmak","meaning":"I became absorbed","explanation":"Watching TV absorbed his attention.","otherMeanings":["to dive"]})";
                const auto body = QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"finish_reason", "stop"},
                    {"message", QJsonObject{{"content", content}}}}}}}).toJson();
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body.left(10));
                QTimer::singleShot(150, socket, [socket, body] { socket->write(body.mid(10)); socket->disconnectFromHost(); });
            });
        });
        QString credentialEndpoint;
        StudyServices study(directory.path(), [&](const QString &endpoint) { credentialEndpoint = endpoint; return QByteArray("synthetic-test-key"); });
        const auto endpoint = QString("http://127.0.0.1:%1").arg(server.serverPort());
        QVERIFY(study.settings()->configure(endpoint, "mock", "https://example.org/%s", false));
        QCOMPARE(study.settings()->learningSettings().value("wordProvider").toString(), QString("ai"));
        const QJsonObject context{{"clickedSubtitle", "Ben televizyona dalmışım ya."},
            {"before", QJsonArray{"Açık bile değil televizyon."}}, {"after", QJsonArray{"Öyle mi?"}},
            {"secondary", QJsonArray{"I was watching TV."}}, {"explanationLanguage", "English"}};
        study.words()->lookupContext("dalmışım", context, study.settings()->endpoint(), study.settings()->model());
        QTRY_COMPARE(study.words()->state(), QString("complete"));
        QCOMPARE(requests, 1);
        QCOMPARE(credentialEndpoint, endpoint);
        QVERIFY(headers.contains("Bearer synthetic-test-key"));
        QVERIFY(!QJsonDocument(submitted).toJson().contains("synthetic-test-key"));
        QCOMPARE(submitted.value("stream").toBool(), false);
        QCOMPARE(submitted.value("response_format").toObject().value("type").toString(), QString("json_object"));
        const auto sentContext = QJsonDocument::fromJson(submitted.value("messages").toArray().last().toObject().value("content").toString().toUtf8()).object();
        QCOMPARE(sentContext.value("word").toString(), QString("dalmışım"));
        QCOMPARE(sentContext.value("before"), context.value("before"));
        QCOMPARE(sentContext.value("after"), context.value("after"));
        QCOMPARE(sentContext.value("secondary"), context.value("secondary"));
        QCOMPARE(study.words()->lemma(), QString("dalmak"));
        QCOMPARE(study.words()->otherMeanings(), QStringList{"to dive"});
        study.words()->lookupContext("dalmışım", context, study.settings()->endpoint(), study.settings()->model());
        QTRY_COMPARE(requests, 2);
        study.words()->cancel();
        QTest::qWait(250);
        QCOMPARE(study.words()->state(), QString("idle"));
        QVERIFY(study.words()->otherMeanings().isEmpty());
        QCOMPARE(requests, 2);
        study.words()->lookupContext("ışık", context, "https://key@invalid.example", "mock");
        QCOMPARE(study.words()->state(), QString("failed"));
        QCOMPARE(requests, 2);
    }
    void appearancePreviewsCanBeCancelledOrApplied() {
        QTemporaryDir directory;
        SubtitleAppearance style(directory.path());
        auto values = style.snapshot();
        values.insert("secondaryColor", "#ffeeaa");
        values.insert("gap", 24);
        values.insert("reversed", true);
        QVERIFY(style.preview(values));
        QCOMPARE(style.secondaryColor(), QString("#ffeeaa"));
        SubtitleAppearance unchanged(directory.path());
        QCOMPARE(unchanged.gap(), 12);
        style.cancelPreview();
        QCOMPARE(style.gap(), 12);
        QVERIFY(style.preview(values));
        QVERIFY(style.applyPreview());
        SubtitleAppearance restored(directory.path());
        QCOMPARE(restored.gap(), 24);
        QVERIFY(restored.reversed());
    }
    void wordExtractionAndResponsesStayPlainAndBounded() {
        QCOMPARE(WordLookup::wordAt("Bir ışık gördüm.", 5), QString("ışık"));
        QCOMPARE(WordLookup::wordAt("日本語 العربية", 1), QString("日本語"));
        QCOMPARE(WordLookup::languageCode("Turkish"), QString("tr"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::languageCode("bad/host"));
        QCOMPARE(WordLookup::parseMeanings(R"({"data":{"translations":[{"translatedText":"light &amp; bright"}]}})", true, false), QStringList{"light & bright"});
        QCOMPARE(WordLookup::parseMeanings(R"([{"translations":[{"displayTarget":"light"},{"displayTarget":"light"},{"displayTarget":"brightness"}]}])", false, true), (QStringList{"light", "brightness"}));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseMeanings(QByteArray(64001, 'x'), false, false));
    }
    void learningSettingsWhitelistFieldsAndNeverPersistKeys() {
        QTemporaryDir directory;
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        const QVariantMap settings{{"sourceLanguage", "Turkish"}, {"explanationLanguage", "English"},
            {"includeSecondary", true}, {"primaryProvider", "configured"}, {"wordProvider", "off"}, {"apiKey", "should-not-save"}};
        QVERIFY(study.settings()->configureLearning(settings));
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains("should-not-save"));
        StudyServices restored(directory.path(), [](const QString &) { return QByteArray(); });
        QCOMPARE(restored.settings()->sourceLanguage(), QString("Turkish"));
        QVERIFY(restored.settings()->includeSecondary());

        QString savedEndpoint;
        study.settings()->setCredentialWriter([&](const QString &endpoint, QByteArray key) {
            savedEndpoint = endpoint;
            const bool valid = key == "synthetic-test-key";
            key.fill(0);
            return valid ? QString("Saved") : QString("Invalid");
        });
        study.settings()->saveServiceKey("google", "synthetic-test-key");
        QCOMPARE(savedEndpoint, QString("https://translation.googleapis.com"));
        QCOMPARE(study.settings()->message(), QString("Saved"));
        QVERIFY(!study.settings()->learningSettings().contains("apiKey"));
    }

    void optedInFallbackWaitsAndUsesOnlyOneAlternateAttempt() {
        QTemporaryDir directory;
        QTcpServer primary;
        QTcpServer alternate;
        QVERIFY(primary.listen(QHostAddress::LocalHost));
        QVERIFY(alternate.listen(QHostAddress::LocalHost));
        int primaryRequests = 0;
        int alternateRequests = 0;
        connect(&primary, &QTcpServer::newConnection, this, [&] {
            auto *socket = primary.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("counted").toBool()) return;

                socket->setProperty("counted", true);
                ++primaryRequests;
            });
        });
        connect(&alternate, &QTcpServer::newConnection, this, [&] {
            auto *socket = alternate.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("sent").toBool()) return;

                socket->setProperty("sent", true);
                ++alternateRequests;
                const QByteArray answer = "data: {\"choices\":[{\"delta\":{\"content\":\"Light\"}}]}\n\ndata: [DONE]\n\n";
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(answer.size()) + "\r\nConnection: close\r\n\r\n" + answer);
                socket->disconnectFromHost();
            });
        });
        const QString local = QString("http://127.0.0.1:%1");
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr,
            "https://api.opensubtitles.com/api/v1", local.arg(alternate.serverPort()));
        QVERIFY(study.settings()->configure(local.arg(primary.serverPort()), "mock", "https://example.org/%s", false));
        QVariantMap settings{{"sourceLanguage", "Turkish"}, {"explanationLanguage", "English"}, {"wordProvider", "off"},
            {"primaryProvider", "configured"}, {"fallbackEnabled", true}};
        QVERIFY(study.settings()->configureLearning(settings));
        study.explanation()->explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE(primaryRequests, 1);
        QTest::qWait(100);
        QCOMPARE(alternateRequests, 0);
        QTRY_COMPARE_WITH_TIMEOUT(study.explanation()->state(), QString("complete"), 7000);
        QCOMPARE(primaryRequests, 1);
        QCOMPARE(alternateRequests, 1);
        QCOMPARE(study.explanation()->turns().first().toMap().value("fallback").toBool(), true);
        QCOMPARE(study.explanation()->turns().first().toMap().value("provider").toString(), QString("gpt-6-luna"));
    }
    void wordLookupStreamsOneBoundedResponseAndCancellationRejectsLateCallbacks() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("sent").toBool()) return;

                socket->setProperty("sent", true);
                ++requests;
                const QByteArray body = R"({"data":{"translations":[{"translatedText":"light"}]}})";
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body.left(10));
                QTimer::singleShot(150, socket, [socket, body] { socket->write(body.mid(10)); socket->disconnectFromHost(); });
            });
        });
        WordLookup words([](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr,
            QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort())));
        const QVariantMap settings{{"wordProvider", "google"}, {"sourceLanguage", "Turkish"}, {"explanationLanguage", "English"}};
        words.lookup("ışık", settings);
        QTRY_COMPARE(words.state(), QString("complete"));
        QCOMPARE(words.meanings(), QStringList{"light"});
        QCOMPARE(requests, 1);
        words.lookup("ışık", settings);
        QTRY_COMPARE(requests, 2);
        words.cancel();
        QTest::qWait(200);
        QCOMPARE(words.state(), QString("idle"));
        QVERIFY(words.meanings().isEmpty());
    }
    void appearanceIsBoundedAndPersistsSeparately() {
        QTemporaryDir directory;
        SubtitleAppearance style(directory.path());
        QCOMPARE(style.primarySize(), 28);
        QCOMPARE(style.secondarySize(), 24);
        QVERIFY(style.outline());
        QVERIFY(!style.background());
        QVERIFY(!style.configure(0, 24, 36, true, false, true, "#ffffff"));
        QVERIFY(!style.configure(28, 24, 36, true, false, true, "red;script"));
        QVERIFY(style.configure(32, 26, 48, false, true, false, "#fff0cc"));
        SubtitleAppearance restored(directory.path());
        QCOMPARE(restored.primarySize(), 32);
        QCOMPARE(restored.secondarySize(), 26);
        QCOMPARE(restored.bottomMargin(), 48);
        QCOMPARE(restored.textColor(), QString("#fff0cc"));
        QVERIFY(restored.background());
        QVERIFY(!restored.outline());
        QVERIFY(!QFile::exists(directory.filePath("recent-videos.json")));
        restored.reset();
        QCOMPARE(restored.primarySize(), 28);
        QVERIFY(!restored.background());
    }
    void networkErrorsGiveSpecificRecovery() {
        QVERIFY(networkFailureMessage(0, QNetworkReply::HostNotFoundError, "Explanation failed").contains("server could not be found"));
        QVERIFY(networkFailureMessage(0, QNetworkReply::SslHandshakeFailedError, "Explanation failed").contains("secure connection"));
        QVERIFY(networkFailureMessage(401, QNetworkReply::AuthenticationRequiredError, "Explanation failed").contains("Settings"));
        QVERIFY(networkFailureMessage(429, QNetworkReply::UnknownContentError, "Explanation failed").contains("allowance"));
        QVERIFY(networkFailureMessage(503, QNetworkReply::ServiceUnavailableError, "Explanation failed").contains("temporarily unavailable"));
    }
    void failedRequestDoesNotRetryAndAnExplicitRetryCanSucceed() {
        QTemporaryDir directory;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("sent").toBool()) return;

                socket->setProperty("sent", true);
                ++requests;
                const auto body = requests == 1 ? QByteArray("{\"error\":\"private provider details\"}")
                    : QByteArray("data: {\"choices\":[{\"delta\":{\"content\":\"Light\"}}]}\n\ndata: [DONE]\n\n");
                const auto status = requests == 1 ? QByteArray("429 Too Many Requests") : QByteArray("200 OK");
                socket->write("HTTP/1.1 " + status + "\r\nConnection: close\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock", "https://example.org/%s", false));
        study.explanation()->explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE(study.explanation()->state(), QString("failed"));
        QVERIFY(study.explanation()->message().contains("allowance"));
        QVERIFY(!study.explanation()->message().contains("private provider details"));
        QVERIFY(!study.explanation()->message().contains("HTTP 0"));
        QTest::qWait(200);
        QCOMPARE(requests, 1);
        study.explanation()->retryExplanation();
        QTRY_COMPARE(study.explanation()->state(), QString("complete"));
        QCOMPARE(requests, 2);
        QCOMPARE(study.explanation()->subject(), QString("ışık"));
        QCOMPARE(study.explanation()->answer(), QString("Light"));
    }
    void dictionaryEncodingAndDestinationValidation() {
        const auto url = AppSettings::buildDictionaryUrl("https://example.org/?q=%s", "İstanbul & ışık?#");
        QCOMPARE(QUrlQuery(url).queryItemValue("q", QUrl::FullyDecoded), QString("İstanbul & ışık?#"));
        QVERIFY(AppSettings::buildDictionaryUrl("https://%s.example.org", "evil").isEmpty());
        QVERIFY(AppSettings::buildDictionaryUrl("javascript:%s", "test").isEmpty());
        QVERIFY(AppSettings::buildDictionaryUrl("https://user:pass@example.org/%s", "test").isEmpty());
    }
    void persistentCardsAndLiteralExport() {
        QTemporaryDir directory;
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        study.deck()->saveCard("ışık", "<script>context</script>\nline", "light\tbright", 1.5);
        QCOMPARE(study.deck()->cards().size(), 1);
        study.deck()->exportCards(QUrl::fromLocalFile(directory.filePath("cards.tsv")));
        QFile exportFile(directory.filePath("cards.tsv"));
        QVERIFY(exportFile.open(QIODevice::ReadOnly));
        const auto text = exportFile.readAll();
        QVERIFY(text.contains("#html:false"));
        QVERIFY(text.contains("light bright"));
        StudyServices restored(directory.path(), [](const QString &) { return QByteArray(); });
        QCOMPARE(restored.deck()->cards().size(), 1);
        restored.deck()->reviewCard(0, true);
        QCOMPARE(restored.deck()->cards().first().toMap().value("reviews").toInt(), 1);
        restored.deck()->removeCard(0);
        QCOMPARE(restored.deck()->cards().size(), 0);
    }
    void corruptStorageIsPreserved() {
        QTemporaryDir directory;
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("broken data"); file.close();
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        study.deck()->saveCard("ışık", "", "light", 0);
        QCOMPARE(study.deck()->cards().size(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("broken data"));
    }
    void invalidCardSchemaIsPreserved() {
        QTemporaryDir directory;
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray data = R"({"version":1,"cards":[{"front":"ışık"}]})";
        file.write(data); file.close();
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        study.deck()->saveCard("ışık", "", "light", 0);
        QCOMPARE(study.deck()->cards().size(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);
    }
    void knownMovieHash() {
        QTemporaryDir directory;
        QFile file(directory.filePath("video"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(131072, '\0')); file.close();
        QCOMPARE(SubtitleService::movieHash(file.fileName()), QString("0000000000020000"));
    }
    void missingCredentialSendsNothing() {
        QTemporaryDir directory;
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        study.explanation()->explain("ışık", "Bir ışık gördüm.");
        QCOMPARE(study.explanation()->state(), QString("failed"));
        QVERIFY(study.explanation()->answer().isEmpty());
    }
    void subtitleSearchDownloadAndCredentialIsolation() {

        // 1. Serve the search, login, download and file protocol using only synthetic media and credentials.
        QTemporaryDir directory;
        QFile media(directory.filePath("video"));
        QVERIFY(media.open(QIODevice::WriteOnly));
        media.write(QByteArray(131072, '\0')); media.close();
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const auto base = QString("http://127.0.0.1:%1").arg(server.serverPort());
        QStringList requests;
        bool apiAuthenticated = true;
        bool fileUnauthenticated = false;
        bool downloadAuthenticated = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            auto bytes = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket, bytes] {
                *bytes += socket->readAll();
                if (!bytes->contains("\r\n\r\n") || socket->property("sent").toBool()) return;
                socket->setProperty("sent", true);
                const auto target = QString::fromUtf8(bytes->split(' ').value(1));
                requests.append(target);
                if (requests.size() == 1) {
                    socket->write("HTTP/1.1 301 Moved\r\nLocation: " + target.toUtf8() + "&canonical=1\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                QByteArray body;
                if (target.startsWith("/subtitles?")) {
                    apiAuthenticated &= bytes->toLower().contains("api-key: synthetic-test-key");
                    body = R"({"data":[{"attributes":{"release":"Original fixture","language":"tr","moviehash_match":true,"files":[{"file_id":42}]}}]})";
                } else if (target == "/login") {
                    apiAuthenticated &= bytes->toLower().contains("api-key: synthetic-test-key");
                    body = R"({"token":"synthetic-session-token","base_url":"127.0.0.1"})";
                } else if (target == "/download") {
                    apiAuthenticated &= bytes->toLower().contains("api-key: synthetic-test-key");
                    downloadAuthenticated = bytes->toLower().contains("authorization: bearer synthetic-session-token");
                    body = ("{\"link\":\"" + base + "/file\"}").toUtf8();
                } else {
                    fileUnauthenticated = !bytes->toLower().contains("api-key:") && !bytes->toLower().contains("authorization:");
                    body = QString("1\n00:00:01,000 --> 00:00:02,000\nİstanbul ışığı\n").toUtf8();
                }
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr, base);
        study.subtitles()->setSubtitleLoginReader([] { return QByteArray(R"({"username":"synthetic-user","password":"synthetic-password"})"); });
        QVector<Cue> received;
        QString savedPath;
        connect(study.subtitles(), &SubtitleService::subtitlesReady, this, [&](QVector<Cue> cues, QString path) {
            received = cues;
            savedPath = path;
        });

        // 2. An exact match may auto-download, but signed file requests never receive the key.
        study.subtitles()->setMedia(media.fileName());
        study.subtitles()->findSubtitles("tr", true);
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(received.first().text, QString("İstanbul ışığı"));
        QFile savedSubtitle(savedPath);
        QVERIFY(savedSubtitle.open(QIODevice::ReadOnly));
        QCOMPARE(parseSrt(QString::fromUtf8(savedSubtitle.readAll())).first().text, QString("İstanbul ışığı"));
        QCOMPARE(requests.size(), 5);
        QVERIFY(requests.first().contains("languages=tr&moviehash=0000000000020000"));
        QVERIFY(apiAuthenticated);
        QVERIFY(fileUnauthenticated);
        QVERIFY(downloadAuthenticated);

        // 3. A manual title/episode search does not inherit the movie hash or auto-download.
        study.subtitles()->findSubtitles("tr", false, "Original show", 2, 1);
        QTRY_COMPARE(requests.size(), 6);
        const QUrlQuery query(QUrl("http://localhost" + requests.last()));
        QCOMPARE(query.queryItemValue("query"), QString("Original show"));
        QCOMPARE(query.queryItemValue("season_number"), QString("2"));
        QCOMPARE(query.queryItemValue("episode_number"), QString("1"));
        QVERIFY(!query.hasQueryItem("moviehash"));

        // 4. Reopening a video with a saved track must not trigger another API request.
        QVERIFY(study.settings()->configure(base, "mock", "https://example.org/%s", true));
        study.subtitles()->setMedia(media.fileName(), false);
        QTest::qWait(100);
        QCOMPARE(requests.size(), 6);
    }
    void subtitleSearchRefusesDifferentPathRedirect() {
        QTemporaryDir directory;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("sent").toBool()) return;

                socket->setProperty("sent", true);
                requests++;
                socket->write("HTTP/1.1 301 Moved\r\nLocation: /unexpected\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                socket->disconnectFromHost();
            });
        });
        const auto base = QString("http://127.0.0.1:%1").arg(server.serverPort());
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr, base);
        study.subtitles()->setMedia(directory.filePath("video"));
        study.subtitles()->findSubtitles("tr", false, "Original show");
        QTRY_VERIFY(study.subtitles()->message().contains("failed"));
        QCOMPARE(requests, 1);
        QVERIFY(study.subtitles()->subtitleResults().isEmpty());
    }
    void incompleteStreams_data() {
        QTest::addColumn<QByteArray>("ending");
        QTest::newRow("missing-done") << QByteArray();
        QTest::newRow("token-limit") << QByteArray("data: {\"choices\":[{\"finish_reason\":\"length\"}]}\n\ndata: [DONE]\n\n");
        QTest::newRow("filtered") << QByteArray("data: {\"choices\":[{\"finish_reason\":\"content_filter\"}]}\n\ndata: [DONE]\n\n");
    }
    void incompleteStreams() {
        QFETCH(QByteArray, ending);
        QTemporaryDir directory;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, ending] {
                socket->readAll();
                if (socket->property("sent").toBool()) return;
                socket->setProperty("sent", true);
                const auto body = QByteArray("data: {\"choices\":[{\"delta\":{\"content\":\"light\"}}]}\n\n") + ending;
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock", "https://example.org/%s", false));
        study.explanation()->explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE(study.explanation()->state(), QString("incomplete"));
        QCOMPARE(study.explanation()->answer(), QString("light"));
    }
    void streamingAndCancellation() {

        // 1. Serve delayed chunks with the Turkish UTF-8 sequence split across writes.
        QTemporaryDir directory;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        bool disconnected = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            auto received = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::disconnected, this, [&] { disconnected = true; });
            connect(socket, &QTcpSocket::readyRead, socket, [socket, received] {
                *received += socket->readAll();
                if (!received->contains("\r\n\r\n") || socket->property("started").toBool()) return;
                socket->setProperty("started", true);
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                const auto event = QString("data: {\"choices\":[{\"delta\":{\"content\":\"ışık\"}}]}\n\n").toUtf8();
                const auto split = event.indexOf(QString("ı").toUtf8()) + 1;
                socket->write(event.left(split));
                QTimer::singleShot(40, socket, [socket, event, split] { socket->write(event.mid(split)); });
                QTimer::singleShot(1000, socket, [socket] { socket->write("data: [DONE]\n\n"); socket->disconnectFromHost(); });
            });
        });
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock", "https://example.org/%s", false));

        // 2. Prove visible text precedes completion, then cancel and reject late output.
        study.explanation()->explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE_WITH_TIMEOUT(study.explanation()->answer(), QString("ışık"), 500);
        QCOMPARE(study.explanation()->state(), QString("streaming"));
        study.explanation()->stop();
        QCOMPARE(study.explanation()->state(), QString("incomplete"));
        QTRY_VERIFY(disconnected);
        study.explanation()->invalidateContext();
        QTest::qWait(1100);
        QVERIFY(study.explanation()->answer().isEmpty());

        // 3. Require a successful end marker on a new request.
        study.explanation()->explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE_WITH_TIMEOUT(study.explanation()->state(), QString("complete"), 2000);
        QCOMPARE(study.explanation()->answer(), QString("ışık"));
    }
};
QTEST_GUILESS_MAIN(StudyTests)
#include "study_tests.moc"
