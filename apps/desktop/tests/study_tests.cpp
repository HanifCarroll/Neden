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

// A loopback server records the actual protocol without contacting providers.
class CatalogServer : public QTcpServer {
public:
    QList<QByteArray> responses;
    QList<QByteArray> requests;
    QByteArray extraHeaders;
    int status = 200;
    int delay = 0;
    CatalogServer() {
        if (!listen(QHostAddress::LocalHost)) qFatal("Cannot start catalog test server");
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                const auto request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                if (!request.contains("\r\n\r\n") || socket->property("sent").toBool()) return;
                socket->setProperty("sent", true);
                requests.append(request);
                const auto body = responses.isEmpty() ? QByteArray("{}") : responses.takeFirst();
                const auto response = "HTTP/1.1 " + QByteArray::number(status) + " Response\r\nConnection: close\r\n"
                    + extraHeaders + "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
                QTimer::singleShot(delay, socket, [socket, response] {
                    if (socket->state() != QAbstractSocket::ConnectedState) return;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            });
        });
    }
    QString origin() const { return QString("http://127.0.0.1:%1").arg(serverPort()); }
};

class StudyTests : public QObject {
    Q_OBJECT
private slots:
    void alignedExplanationKeepsReadableCardsAndNormalFollowUps() {
        QTemporaryDir directory;
        CatalogServer server;
        const QJsonObject answer{{"translation", "Hello!"},
            {"breakdown", QJsonArray{QJsonObject{{"word", "Bonjour"}, {"text", "Hello; a greeting."}}}},
            {"alignment", QJsonArray{QJsonObject{{"text", "Hello"}, {"source", QJsonArray{"Bonjour"}}}}}};
        const auto stream = [](const QString &text) {
            return "data: " + QJsonDocument(QJsonObject{{"choices", QJsonArray{
                QJsonObject{{"delta", QJsonObject{{"content", text}}}}}}}).toJson(QJsonDocument::Compact)
                + "\n\ndata: [DONE]\n\n";
        };
        server.responses = {stream(QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact))),
                            stream("A normal follow-up."), stream("{\"translation\":\"Hello\"")};
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(server.origin(), "mock", "https://example.org/%s", false));
        study.explanation()->explain("Bonjour", "Bonjour");
        QTRY_COMPARE(study.explanation()->state(), QString("complete"));
        const auto readable = study.explanation()->answer();
        QVERIFY(readable.contains("Hello"));
        QVERIFY(!readable.contains("\"alignment\""));
        QVERIFY(!readable.contains("neden-breakdown:"));
        const auto saved = study.deck()->saveCard("Bonjour", "Bonjour", readable, 0);
        QVERIFY(saved.success);
        QVERIFY(study.explanation()->followUp("Is it formal?"));
        QTRY_COMPARE(study.explanation()->state(), QString("complete"));
        QCOMPARE(study.explanation()->answer(), QString("A normal follow-up."));
        study.explanation()->explain("Bonjour", "Bonjour");
        QTRY_COMPARE(study.explanation()->state(), QString("incomplete"));
        QVERIFY(!study.explanation()->answer().contains("{\"translation"));
    }

    void subtitleCatalogSubvaultChoosesTitleAndReleaseWithoutCredentials() {
        CatalogServer server;
        int credentialReads = 0;
        SubtitleCatalog catalog([&](const QString &) { ++credentialReads; return QByteArray("secret"); }, nullptr,
                                server.origin(), server.origin(), server.origin());
        QSignalSpy results(&catalog, &SubtitleCatalog::resultsReady);
        QSignalSpy files(&catalog, &SubtitleCatalog::fileReady);
        server.responses = {
            R"({"shows":[{"id":"show_1","name":"Example show"}]})",
            R"({"matchingSubtitles":[{"completed":true,"release":"Web","downloadUri":"/subtitles/download/sp_entry-1"},{"completed":false,"release":"Incomplete","downloadUri":"/subtitles/download/unfinished"}]})",
            "1\n00:00:01,000 --> 00:00:02,000\nHello.\n"};
        catalog.search("subvault", "Example show", "fr", 2, 3);
        QTRY_COMPARE(results.count(), 1);
        auto titles = results.last().at(0).toList();
        QCOMPARE(titles.size(), 1);
        catalog.select(titles.first().toMap());
        QTRY_COMPARE(results.count(), 3); // Clear titles, then publish releases.
        auto releases = results.last().at(0).toList();
        QCOMPARE(releases.size(), 1);
        catalog.select(releases.first().toMap());
        QTRY_COMPARE(files.count(), 1);
        QVERIFY(files.first().at(0).toByteArray().contains("Hello."));
        QCOMPARE(credentialReads, 0);
        QCOMPARE(server.requests.size(), 3);
        QVERIFY(server.requests.at(1).startsWith("GET /subtitles/get/show_1/2/3/fr "));
        for (const auto &request : server.requests) QVERIFY(!request.toLower().contains("api-key"));
    }

    void subtitleCatalogSubdlFiltersPacksAndKeepsKeysOutOfUrls() {
        CatalogServer server;
        QStringList credentialOrigins;
        SubtitleCatalog catalog([&](const QString &origin) { credentialOrigins.append(origin); return QByteArray("test-secret"); }, nullptr,
                                server.origin(), server.origin(), server.origin());
        QSignalSpy results(&catalog, &SubtitleCatalog::resultsReady);
        QSignalSpy files(&catalog, &SubtitleCatalog::fileReady);
        server.responses = {
            R"({"results":[{"sd_id":123,"name":"Example","type":"tv","year":2024}]})",
            R"({"subtitles":[{"full_season":true,"unpack_files":[{"season":2,"episode":3,"release_name":"Correct","url":"/subtitle/abc/episode3?api_key=leaked"},{"season":2,"episode":4,"release_name":"Wrong episode","url":"/subtitle/abc/episode4"},{"season":2,"episode":3,"release_name":"External","url":"https://attacker.invalid/subtitle/abc"}]}]})",
            "1\n00:00:01,000 --> 00:00:02,000\nBonjour.\n"};
        catalog.search("subdl", "Example", "fr", 2, 3);
        QTRY_COMPARE(results.count(), 1);
        catalog.select(results.last().at(0).toList().first().toMap());
        QTRY_COMPARE(results.count(), 3);
        const auto releases = results.last().at(0).toList();
        QCOMPARE(releases.size(), 1);
        const auto release = releases.first().toMap();
        QCOMPARE(release.value("release").toString(), QString("Correct"));
        QVERIFY(!release.value("download").toString().contains("api_key"));
        catalog.select(release);
        QTRY_COMPARE(files.count(), 1);
        QCOMPARE(credentialOrigins, QStringList({server.origin(), server.origin(), server.origin()}));
        QVERIFY(server.requests.at(1).contains("unpack=1"));
        QVERIFY(server.requests.at(2).startsWith("GET /subtitle/abc/episode3 HTTP"));
        for (const auto &request : server.requests) {
            QVERIFY(request.toLower().contains("x-api-key: test-secret"));
            QVERIFY(!request.contains("leaked"));
            QVERIFY(!request.split('\n').first().contains("test-secret"));
        }
    }

    void subtitleCatalogMissingKeyAndRateLimitsAreActionable() {
        CatalogServer server;
        SubtitleCatalog catalog([](const QString &) { return QByteArray(); }, nullptr,
                                server.origin(), server.origin(), server.origin());
        QSignalSpy messages(&catalog, &SubtitleCatalog::messageChanged);
        catalog.search("subdl", "Example", "en", 0, 0);
        QVERIFY(messages.last().first().toString().contains("SubDL key"));
        QVERIFY(server.requests.isEmpty());
        server.status = 429;
        server.responses = {"{}"};
        catalog.search("subvault", "Example", "en", 1, 1);
        QTRY_VERIFY(messages.last().first().toString().contains("rate limit"));
        server.status = 423;
        server.responses = {"{}"};
        catalog.search("subvault", "Example", "en", 1, 1);
        QTRY_VERIFY(messages.last().first().toString().contains("refreshing"));
    }

    void subtitleCatalogCancellationAndRedirectsNeverLeakCredentials() {
        CatalogServer server;
        SubtitleCatalog catalog([](const QString &) { return QByteArray("private"); }, nullptr,
                                server.origin(), server.origin(), server.origin());
        QSignalSpy results(&catalog, &SubtitleCatalog::resultsReady);
        QSignalSpy messages(&catalog, &SubtitleCatalog::messageChanged);
        server.responses = {R"({"results":[{"sd_id":1,"name":"Old"}]})"};
        server.delay = 100;
        catalog.search("subdl", "Example", "en", 0, 0);
        QTRY_COMPARE(server.requests.size(), 1);
        catalog.cancel();
        QTest::qWait(150);
        QVERIFY(results.isEmpty());
        server.delay = 0;
        server.status = 302;
        server.extraHeaders = "Location: https://attacker.invalid/collect\r\n";
        server.responses = {"{}"};
        catalog.search("subdl", "Example", "en", 0, 0);
        QTRY_VERIFY(messages.last().first().toString().contains("failed"));
        QCOMPARE(server.requests.size(), 2);
        QVERIFY(results.isEmpty());
    }

    void languagePreferencesUseValidatedChoicesAndNeutralDefaults() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        QCOMPARE(settings.sourceLanguageCode(), QString("auto"));
        QCOMPARE(settings.explanationLanguage(), LanguageCatalog::systemLanguage());
        QVERIFY(settings.sourceLanguages().size() > 100);
        QVERIFY(settings.subtitleLanguages().size() > 50);
        QSet<QString> languageIds;
        for (const auto &option : settings.sourceLanguages()) {
            const auto id = option.toMap().value("id").toString();
            QVERIFY2(!languageIds.contains(id), qPrintable(id));
            languageIds.insert(id);
        }
        QVERIFY(settings.configureLearning({{"sourceLanguage", "es"}, {"explanationLanguage", "French"}}));
        QCOMPARE(settings.sourceLanguage(), QString("Spanish"));
        QCOMPARE(settings.explanationLanguageCode(), QString("fr"));
        QCOMPARE(settings.subtitleLanguageCode(), QString("es"));
        const auto saved = settings.learningSettings();
        QVERIFY(!settings.configureLearning({{"sourceLanguage", "invented language"}}));
        QVERIFY(!settings.configureLearning({{"explanationLanguage", "auto"}}));
        QCOMPARE(settings.learningSettings(), saved);
        QCOMPARE(LanguageCatalog::subtitleCode("Portuguese (Brazil)"), QString("pt-br"));
        QCOMPARE(LanguageCatalog::subtitleCode("Azerbaijani"), QString("az-az"));
        QCOMPARE(LanguageCatalog::subtitleCode("auto"), QString());
        QVERIFY(!LanguageCatalog::supportsSubtitleCode("anything"));
        QCOMPARE(LanguageCatalog::subtitleLocaleCode("pt-pt"), QString("pt"));
        QCOMPARE(LanguageCatalog::subtitleLocaleCode("az-az"), QString("az"));
        QCOMPARE(LanguageCatalog::subtitleLocaleCode("ma"), QString("mni"));
        QCOMPARE(LanguageCatalog::subtitleLocaleCode("pt-br"), QString("pt-br"));
    }

    void legacyFreeTextLanguagesMigrateWithoutLosingOtherPreferences() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        QVERIFY(profile.update({{"learning", QJsonObject{{"sourceLanguage", "not a language"},
                                                         {"explanationLanguage", "fr"},
                                                         {"wordProvider", "off"},
                                                         {"includeSecondary", false}}}}));
        AppSettings settings(profile);
        QCOMPARE(settings.sourceLanguageCode(), QString("auto"));
        QCOMPARE(settings.explanationLanguage(), QString("French"));
        QCOMPARE(settings.wordProvider(), QString("off"));
        QVERIFY(!settings.includeSecondary());
    }

    void automaticSubtitlesFollowSelectedLanguageAndSkipAutoDetection() {
        QTemporaryDir directory;
        QFile media(directory.filePath("video.mkv"));
        QVERIFY(media.open(QIODevice::WriteOnly));
        media.write(QByteArray(131072, '\0'));
        media.close();
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QStringList languages;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                auto bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                if (!bytes.contains("\r\n\r\n") || socket->property("sent").toBool())
                    return;
                socket->setProperty("sent", true);
                const QUrl url("http://localhost" + QString::fromUtf8(bytes.split(' ').value(1)));
                languages.append(QUrlQuery(url).queryItemValue("languages"));
                const QByteArray body = R"({"data":[]})";
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        const auto base = QString("http://127.0.0.1:%1").arg(server.serverPort());
        StudyServices study(directory.path(), [](const QString &) { return QByteArray("test-key"); }, nullptr, base);
        QVERIFY(study.settings()->configure(base, "test", "https://example.org/%s", true));
        study.subtitles()->setMedia(media.fileName());
        QTest::qWait(50);
        QVERIFY(languages.isEmpty());
        QVERIFY(study.settings()->configureLearning({{"sourceLanguage", "Japanese"}}));
        study.subtitles()->setMedia(media.fileName());
        QTRY_COMPARE(languages, QStringList{"ja"});
        QVERIFY(study.settings()->configureLearning({{"sourceLanguage", "pt-BR"}}));
        study.subtitles()->setMedia(media.fileName());
        QTRY_COMPARE(languages, (QStringList{"ja", "pt-br"}));
        study.subtitles()->findSubtitles("fr", false, "Example film");
        QTRY_COMPARE(languages, (QStringList{"ja", "pt-br", "fr"}));
        study.subtitles()->setMedia(media.fileName(), false);
        study.subtitles()->findSubtitles("invalid", false, "Example film");
        QTest::qWait(50);
        QCOMPARE(languages.size(), 3);
    }

    void settingsDraftCommitsAllFieldsOnlyAfterSuccessfulPersistence() {
        QTemporaryDir directory;
        StudyServices services(directory.path(), [](const QString &) { return QByteArray(); });
        auto *settings = services.settings();
        const auto originalEndpoint = settings->endpoint();
        auto draft = settings->learningSettings();
        draft.insert("endpoint", "https://example.org/v1");
        draft.insert("model", "new-model");
        draft.insert("dictionary", "https://example.org/dictionary/%s");
        draft.insert("automaticSubtitles", true);
        draft.insert("wordProvider", "unknown");
        QSignalSpy changes(settings, &AppSettings::changed);
        QVERIFY(!settings->applyDraft(draft));
        QCOMPARE(settings->endpoint(), originalEndpoint);
        QCOMPARE(changes.count(), 0);
        draft.insert("wordProvider", "ai");
        QVERIFY(settings->applyDraft(draft));
        QCOMPARE(settings->endpoint(), QString("https://example.org/v1"));
        QCOMPARE(settings->model(), QString("new-model"));
        QCOMPARE(changes.count(), 1);
        const auto savedValues = settings->learningSettings();
        QVERIFY(QFile::rename(directory.filePath("study.json"), directory.filePath("preserved.json")));
        QVERIFY(QDir().mkdir(directory.filePath("study.json"))); // Force atomic replacement to fail.
        draft.insert("endpoint", "https://different.example/v1");
        draft.insert("sourceLanguage", "Japanese");
        QVERIFY(!settings->applyDraft(draft));
        QCOMPARE(settings->endpoint(), QString("https://example.org/v1"));
        QCOMPARE(settings->learningSettings(), savedValues);
        QCOMPARE(changes.count(), 1);
    }

    void featureStatusMessagesDoNotOverwriteAnExplanation() {
        QTemporaryDir directory;
        StudyServices services(directory.path(), [](const QString &) { return QByteArray(); });
        services.explanation()->explain("ışık", "Bir ışık gördüm.");
        QCOMPARE(services.explanation()->state(), QString("failed"));
        const auto error = services.explanation()->message();
        services.deck()->saveCard("ışık", "Bir ışık gördüm.", "light", 1.5);
        QVERIFY(services.deck()->message().contains("saved"));
        QCOMPARE(services.explanation()->message(), error);
        services.subtitles()->findSubtitles("invalid");
        QCOMPARE(services.explanation()->message(), error);
        QVERIFY(!services.subtitles()->message().isEmpty());
        StudyServices restored(directory.path(), [](const QString &) { return QByteArray(); });
        QCOMPARE(restored.deck()->cards().size(), 1);
    }

    void contextualWordCardsRequireCompleteBoundedStructuredAnswers() {
        const auto envelope = [](const QJsonObject &card, const QString &finish = "stop") {
            return QJsonDocument(
                       QJsonObject{
                           {"choices",
                            QJsonArray{QJsonObject{
                                {"finish_reason", finish},
                                {"message", QJsonObject{{"content", QString::fromUtf8(
                                                                        QJsonDocument(card).toJson())}}}}}}})
                .toJson();
        };
        QJsonObject card{{"lemma", "dalmak"},
                         {"partOfSpeech", "verb"},
                         {"meaning", "I became absorbed"},
                         {"explanation", "The speaker was absorbed in watching television."},
                         {"formNote", "Dalmışım is an inflected form of dalmak."},
                         {"otherSenses", QJsonArray{QJsonObject{{"meaning", "to dive into water"},
                             {"example", "suya dalmak"}, {"translation", "to dive into the water"},
                             {"formNote", "Uses the infinitive dalmak."}}}},
                         {"otherUses", QJsonArray{}}};
        const auto parsed = WordLookup::parseContextMeaning(envelope(card));
        QCOMPARE(parsed.value("lemma").toString(), QString("dalmak"));
        QCOMPARE(parsed.value("partOfSpeech").toString(), QString("verb"));
        card.insert("partOfSpeech", QJsonArray{});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.insert("partOfSpeech", QString(61, 'x'));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.remove("partOfSpeech");
        QVERIFY(WordLookup::parseContextMeaning(envelope(card)).value("partOfSpeech").toString().isEmpty());
        QCOMPARE(parsed.value("meaning").toString(), QString("I became absorbed"));
        QCOMPARE(parsed.value("otherSenses").toList().size(), 1);
        QCOMPARE(parsed.value("otherSenses").toList().first().toMap().value("translation").toString(),
                 QString("to dive into the water"));
        QVERIFY(parsed.value("otherUses").toList().isEmpty());
        QVERIFY(!parsed.value("formNote").toString().isEmpty());
        const auto originalSenses = card.value("otherSenses");
        card.insert("otherSenses", QJsonArray{"unstructured gloss"});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.insert("otherSenses", QJsonArray{QJsonObject{{"meaning", "different sense"}, {"example", "example"}}});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.insert("otherSenses", originalSenses);
        auto duplicate = originalSenses.toArray().first().toObject();
        duplicate.insert("meaning", " I BECAME ABSORBED ");
        card.insert("otherSenses", QJsonArray{duplicate, originalSenses.toArray().first()});
        QCOMPARE(WordLookup::parseContextMeaning(envelope(card)).value("otherSenses").toList().size(), 1);
        card.insert("otherUses", QJsonArray{duplicate});
        QCOMPARE(WordLookup::parseContextMeaning(envelope(card)).value("otherUses").toList().size(), 1);
        card.insert("otherUses", QJsonArray{duplicate, duplicate});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.insert("otherUses", QJsonArray{});
        card.insert("formNote", QJsonArray{});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.insert("formNote", "");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 WordLookup::parseContextMeaning(envelope(card, "length")));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning("{}"));
        card.insert("otherSenses", QJsonArray{});
        QVERIFY(
            WordLookup::parseContextMeaning(envelope(card)).value("otherSenses").toList().isEmpty());
        card.insert("meaning", QString(601, 'x'));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(envelope(card)));
        card.insert("meaning", "<img src='https://invalid.example/tracker'>");
        // Returned text is never interpreted as HTML by the card.
        QVERIFY(
            WordLookup::parseContextMeaning(envelope(card)).value("meaning").toString().startsWith("<img"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, WordLookup::parseContextMeaning(QByteArray(64001, 'x')));
    }

    void wordBatchStreamsOnlyCompleteValidatedEntries() {
        const auto frame = [](const QByteArray &content, const QString &finish = {}) {
            QJsonObject choice{{"delta", QJsonObject{{"content", QString::fromUtf8(content)}}}};
            if (!finish.isEmpty()) choice.insert("finish_reason", finish);
            return "data: " + QJsonDocument(QJsonObject{{"choices", QJsonArray{choice}}}).toJson(QJsonDocument::Compact) + "\n\n";
        };
        WordBatch batch;
        batch.reset({"café", "你好"});
        const auto first = frame("{\"entries\":[{\"word\":\"café\",\"partOfSpeech\":\"noun\",\"meaning\":\"coffee\"}");
        for (char byte : first) batch.append(QByteArray(1, byte));
        QCOMPARE(batch.entries().size(), 1);
        QVERIFY(!batch.complete());
        batch.append(frame(",{\"word\":\"你好\",\"partOfSpeech\":\"greeting\",\"meaning\":\"hello\"}]}"));
        QCOMPARE(batch.entries().size(), 2);
        batch.append(frame({}, "stop") + "data: [DONE]\n\n");
        QVERIFY(batch.complete());
        const QList<QByteArray> invalid{
            "{\"entries\":[{\"word\":\"unknown\",\"meaning\":\"x\",\"partOfSpeech\":\"noun\"}]}",
            "{\"entries\":[{\"word\":\"café\",\"meaning\":42,\"partOfSpeech\":\"noun\"}]}",
            "{\"entries\":[{\"word\":\"café\",\"meaning\":\"x\",\"partOfSpeech\":\"noun\"},{\"word\":\"café\",\"meaning\":\"y\",\"partOfSpeech\":\"noun\"}]}"
        };
        for (const auto &content : invalid) {
            batch.reset({"café"});
            batch.append(frame(content));
            QVERIFY_THROWS_EXCEPTION(std::runtime_error, batch.entries());
        }
        batch.reset({"café"});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, batch.append("data: nonsense\n"));
        batch.reset({"café"});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, batch.append(frame({}, "length")));
        batch.reset({"café"});
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, batch.append(QByteArray(256001, 'x')));
    }

    void wordLookupsShareStreamingLineCacheAndFetchDetailsOnDemand() {
        QTemporaryDir directory;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        bool truncate = false;
        QList<QJsonObject> submitted;
        const auto frame = [](const QByteArray &content, const QString &finish = {}) {
            QJsonObject choice{{"delta", QJsonObject{{"content", QString::fromUtf8(content)}}}};
            if (!finish.isEmpty()) choice.insert("finish_reason", finish);
            return "data: " + QJsonDocument(QJsonObject{{"choices", QJsonArray{choice}}}).toJson(QJsonDocument::Compact) + "\n\n";
        };
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
                const auto request = QJsonDocument::fromJson(bytes->mid(split + 4, length)).object();
                submitted.append(request);
                const auto data = QJsonDocument::fromJson(request.value("messages").toArray().last().toObject().value("content").toString().toUtf8()).object();
                QByteArray first, rest;
                if (request.value("stream").toBool()) {
                    const auto words = data.value("words").toArray();
                    for (int index = 0; index < words.size(); ++index) {
                        if (truncate && index > 0) break;
                        const auto card = QJsonDocument(QJsonObject{{"word", words[index]}, {"partOfSpeech", "verb"}, {"meaning", "meaning of " + words[index].toString()}}).toJson(QJsonDocument::Compact);
                        if (index == 0) first = frame("{\"entries\":[" + card);
                        else rest += frame("," + card);
                    }
                    rest += truncate ? frame({}, "length") : frame("]}") + frame({}, "stop") + "data: [DONE]\n\n";
                } else {
                    const auto card = QJsonDocument(QJsonObject{{"lemma", "dalmak"}, {"meaning", "absorbed"}, {"explanation", "Attention absorbed."}, {"formNote", "Past tense."}}).toJson(QJsonDocument::Compact);
                    first = QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"finish_reason", "stop"}, {"message", QJsonObject{{"content", QString::fromUtf8(card)}}}}}}}).toJson(QJsonDocument::Compact);
                }
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " + QByteArray::number(first.size() + rest.size()) + "\r\n\r\n" + first);
                QTimer::singleShot(250, socket, [socket, rest] { socket->write(rest); socket->disconnectFromHost(); });
            });
        });
        WordLookup lookup([](const QString &) { return QByteArray(); });
        const auto endpoint = QString("http://127.0.0.1:%1").arg(server.serverPort());
        QJsonObject context{{"clickedSubtitle", "Ben dalmışım."}, {"sourceCue", "Ben dalmışım."}, {"learningLanguage", "tr"}, {"explanationLanguage", "en"}};
        lookup.lookupContext("dalmışım", context, endpoint, "local-test", true);
        QTRY_COMPARE(lookup.state(), QString("complete"));
        QCOMPARE(requests, 1);
        QVERIFY(submitted.first().value("stream").toBool());
        QCOMPARE(lookup.detailsState(), QString("idle"));
        QCOMPARE(lookup.formNote(), QString());
        // The second word hasn't arrived; switching must keep the first request alive.
        context.insert("selection", "Ben");
        lookup.lookupContext("Ben", context, endpoint, "local-test", true);
        QCOMPARE(lookup.state(), QString("loading"));
        QTRY_COMPARE(lookup.state(), QString("complete"));
        QCOMPARE(requests, 1);
        lookup.cancel();
        lookup.lookupContext("dalmışım", context, endpoint, "local-test", true);
        QCOMPARE(lookup.state(), QString("complete"));
        QCOMPARE(requests, 1);
        lookup.loadDetails();
        QTRY_COMPARE(lookup.detailsState(), QString("complete"));
        QCOMPARE(requests, 2);
        QVERIFY(!submitted.last().value("stream").toBool());
        QCOMPARE(lookup.formNote(), QString("Past tense."));
        QCOMPARE(lookup.meanings().first(), QString("meaning of dalmışım"));
        lookup.lookupContext("Ben", context, endpoint, "local-test", true);
        lookup.lookupContext("dalmışım", context, endpoint, "local-test", true);
        lookup.loadDetails();
        QCOMPARE(lookup.detailsState(), QString("complete"));
        QCOMPARE(requests, 2);
        context.insert("explanationLanguage", "es");
        lookup.lookupContext("dalmışım", context, endpoint, "local-test", true);
        QTRY_COMPARE(requests, 3);
        QTRY_COMPARE(lookup.state(), QString("complete"));
        lookup.lookupContext("dalmışım", context, endpoint, "different-model", true);
        QTRY_COMPARE(requests, 4);
        lookup.cancel();
        QTest::qWait(300);
        QCOMPARE(lookup.state(), QString("idle"));
        QCOMPARE(requests, 4);
        // A partial response keeps completed cards; retry only requests missing words.
        context.insert("clickedSubtitle", "One two.");
        truncate = true;
        lookup.lookupContext("One", context, endpoint, "local-test", true);
        QTRY_COMPARE(lookup.state(), QString("complete"));
        lookup.lookupContext("two", context, endpoint, "local-test", true);
        QTRY_COMPARE(lookup.state(), QString("failed"));
        QCOMPARE(requests, 5);
        lookup.lookupContext("One", context, endpoint, "local-test", true);
        QCOMPARE(lookup.state(), QString("complete"));
        truncate = false;
        lookup.lookupContext("two", context, endpoint, "local-test", true);
        QTRY_COMPARE(lookup.state(), QString("complete"));
        QCOMPARE(requests, 6);
        const auto retryContext = QJsonDocument::fromJson(submitted.last().value("messages").toArray().last().toObject().value("content").toString().toUtf8()).object();
        QCOMPARE(retryContext.value("words").toArray(), QJsonArray{"two"});
        lookup.cancel();
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
    void wordExtractionSupportsUnicode() {
        QCOMPARE(WordLookup::wordAt("Bir ışık gördüm.", 5), QString("ışık"));
        QCOMPARE(WordLookup::wordAt("東京 العربية", 1), QString("東京"));
        QCOMPARE(WordLookup::wordAt("私は勉強しています", 3), QString("勉強"));
        QCOMPARE(WordLookup::wordAt("我喜欢学习中文", 3), QString("学习"));
        QCOMPARE(WordLookup::wordAt("ภาษาไทยเป็นภาษาที่สวยงาม", 8), QString("เป็น"));
        const QString chineseWithEmoji = QString::fromUtf8("🎬我喜欢学习中文");
        WordLookup lookup([](const QString &) { return QByteArray(); });
        const auto range = lookup.wordRangeAt(chineseWithEmoji, 5);
        QCOMPARE(range.value("start").toInt(), 5);
        QCOMPARE(range.value("end").toInt(), 7);
        QCOMPARE(WordLookup::wordAt("I don't know.", 5), QString("don't"));
        QCOMPARE(WordLookup::wordAt("C'est l’homme.", 10), QString("l’homme"));
        QCOMPARE(WordLookup::wordAt("un porte-monnaie", 9), QString("porte-monnaie"));
        QCOMPARE(WordLookup::wordAt("می‌روم", 3), QString("می‌روم"));
        QCOMPARE(WordLookup::wordAt("مرحبًا بالعالم", 2), QString("مرحبًا"));
        QCOMPARE(WordLookup::wordAt("Привет мир", 2), QString("Привет"));
        QCOMPARE(WordLookup::wordAt("नमस्ते दुनिया", 2), QString("नमस्ते"));
        QCOMPARE(WordLookup::wordAt("—Hello!", 3), QString("Hello"));
        QCOMPARE(WordLookup::wordAt("'Hello'", 3), QString("Hello"));
    }
    void learningSettingsWhitelistFieldsAndNeverPersistKeys() {
        QTemporaryDir directory;
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        const QVariantMap settings{{"sourceLanguage", "Turkish"}, {"explanationLanguage", "English"},
                                   {"includeSecondary", true},    {"primaryProvider", "configured"},
                                   {"wordProvider", "off"},       {"apiKey", "should-not-save"}};
        QVERIFY(study.settings()->configureLearning(settings));
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains("should-not-save"));
        StudyServices restored(directory.path(), [](const QString &) { return QByteArray(); });
        QCOMPARE(restored.settings()->sourceLanguage(), QString("Turkish"));
        QVERIFY(restored.settings()->includeSecondary());

        QString savedEndpoint;
        study.settings()->setCredentialWriter([&](const QString &endpoint, QByteArray key, AppSettings::CredentialCompletion completed) {
            savedEndpoint = endpoint;
            const bool valid = key == "synthetic-test-key";
            key.fill(0);
            completed(valid ? QString("Saved") : QString("Invalid"));
        });
        study.settings()->saveServiceKey("ai", "synthetic-test-key");
        QCOMPARE(savedEndpoint, study.settings()->endpoint());
        QCOMPARE(study.settings()->message(), QString("Saved"));
        QVERIFY(!study.settings()->learningSettings().contains("apiKey"));
    }

    void credentialActionsWaitForCompletionAndNeverPersistSecrets() {
        QTemporaryDir directory;
        ProfileStore store(directory.path());
        AppSettings settings(store);
        AppSettings::CredentialCompletion finish;
        int attempts = 0;
        settings.setCredentialWriter([&](const QString &, QByteArray, AppSettings::CredentialCompletion completed) {
            ++attempts;
            finish = std::move(completed);
        });
        settings.saveServiceKey("ai", "synthetic-key");
        settings.saveServiceKey("ai", "another-key");
        QCOMPARE(attempts, 1);
        QVERIFY(settings.message().contains("Saving"));
        finish("Saved securely");
        QCOMPARE(settings.message(), QString("Saved securely"));
        QVERIFY(!QJsonDocument(store.snapshot()).toJson().contains("synthetic-key"));
        settings.saveServiceKey("ai", "synthetic-key");
        QCOMPARE(attempts, 2);
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
                if (socket->property("counted").toBool())
                    return;

                socket->setProperty("counted", true);
                ++primaryRequests;
            });
        });
        connect(&alternate, &QTcpServer::newConnection, this, [&] {
            auto *socket = alternate.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                if (socket->property("sent").toBool())
                    return;

                socket->setProperty("sent", true);
                ++alternateRequests;
                const QByteArray answer =
                    "data: {\"choices\":[{\"delta\":{\"content\":\"Light\"}}]}\n\ndata: [DONE]\n\n";
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(answer.size()) +
                              "\r\nConnection: close\r\n\r\n" + answer);
                socket->disconnectFromHost();
            });
        });
        const QString local = QString("http://127.0.0.1:%1");
        StudyServices study(
            directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr,
            "https://api.opensubtitles.com/api/v1", local.arg(alternate.serverPort()));
        QVERIFY(study.settings()->configure(local.arg(primary.serverPort()), "mock", "https://example.org/%s",
                                            false));
        QVariantMap settings{{"sourceLanguage", "Turkish"},
                             {"explanationLanguage", "English"},
                             {"wordProvider", "off"},
                             {"primaryProvider", "configured"},
                             {"fallbackEnabled", true}};
        QVERIFY(study.settings()->configureLearning(settings));
        study.explanation()->explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE(primaryRequests, 1);
        QTest::qWait(100);
        QCOMPARE(alternateRequests, 0);
        QTRY_COMPARE_WITH_TIMEOUT(study.explanation()->state(), QString("complete"), 7000);
        QCOMPARE(primaryRequests, 1);
        QCOMPARE(alternateRequests, 1);
        QCOMPARE(study.explanation()->turns().first().toMap().value("fallback").toBool(), true);
        QCOMPARE(study.explanation()->turns().first().toMap().value("provider").toString(),
                 QString("gpt-6-luna"));
    }
    void retiredWordProvidersMigrateWithoutLosingOtherSettings() {
        for (const auto &provider : {QString("google"), QString("microsoft")}) {
            QTemporaryDir directory;
            QFile file(directory.filePath("study.json"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonObject{{"version", 1},
                                                 {"cards", QJsonArray{}},
                                                 {"learning", QJsonObject{{"wordProvider", provider},
                                                                          {"wordRegion", "westus"},
                                                                          {"sourceLanguage", "Spanish"},
                                                                          {"explanationLanguage", "French"},
                                                                          {"includeSecondary", false},
                                                                          {"fallbackEnabled", true},
                                                                          {"primaryProvider", "luna"}}}})
                           .toJson());
            file.close();
            StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
            auto *settings = study.settings();
            QCOMPARE(settings->wordProvider(), QString("ai"));
            QCOMPARE(settings->sourceLanguage(), QString("Spanish"));
            QCOMPARE(settings->explanationLanguage(), QString("French"));
            QCOMPARE(settings->primaryProvider(), QString("luna"));
            QVERIFY(settings->fallbackEnabled());
            QVERIFY(!settings->includeSecondary());
            QCOMPARE(settings->wordProviders().size(), 2);
            QCOMPARE(settings->credentialServices().size(), 4);
            bool credentialWritten = false;
            settings->setCredentialWriter([&](const QString &, QByteArray, AppSettings::CredentialCompletion completed) {
                credentialWritten = true;
                completed("Saved");
            });
            settings->saveServiceKey(provider, "unused-key");
            QVERIFY(!credentialWritten);
            QVERIFY(settings->configureLearning(settings->learningSettings()));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto saved = file.readAll();
            QVERIFY(!saved.contains(provider.toUtf8()));
            QVERIFY(!saved.contains("wordRegion"));
        }
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
        QVERIFY(networkFailureMessage(0, QNetworkReply::HostNotFoundError, "Explanation failed")
                    .contains("server could not be found"));
        QVERIFY(networkFailureMessage(0, QNetworkReply::SslHandshakeFailedError, "Explanation failed")
                    .contains("secure connection"));
        QVERIFY(networkFailureMessage(401, QNetworkReply::AuthenticationRequiredError, "Explanation failed")
                    .contains("Settings"));
        QVERIFY(networkFailureMessage(429, QNetworkReply::UnknownContentError, "Explanation failed")
                    .contains("allowance"));
        QVERIFY(networkFailureMessage(503, QNetworkReply::ServiceUnavailableError, "Explanation failed")
                    .contains("temporarily unavailable"));
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
                if (socket->property("sent").toBool())
                    return;

                socket->setProperty("sent", true);
                ++requests;
                const auto body =
                    requests == 1
                        ? QByteArray("{\"error\":\"private provider details\"}")
                        : QByteArray("data: {\"choices\":[{\"delta\":{\"content\":\"Light\"}}]}\n\ndata: "
                                     "[DONE]\n\n");
                const auto status =
                    requests == 1 ? QByteArray("429 Too Many Requests") : QByteArray("200 OK");
                socket->write("HTTP/1.1 " + status + "\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        StudyServices study(directory.path(),
                            [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock",
                                            "https://example.org/%s", false));
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
        const auto result = study.deck()->saveCard("ışık", "<script>context</script>\nline", "light\tbright", 1.5);
        QVERIFY(result.success);
        QCOMPARE(result.message, QString("Flashcard saved on this device."));
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
        file.write("broken data");
        file.close();
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        const auto result = study.deck()->saveCard("ışık", "", "light", 0);
        QVERIFY(!result.success);
        QVERIFY(!result.message.isEmpty());
        QCOMPARE(study.deck()->cards().size(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("broken data"));
    }
    void invalidCardSchemaIsPreserved() {
        QTemporaryDir directory;
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray data = R"({"version":1,"cards":[{"front":"ışık"}]})";
        file.write(data);
        file.close();
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        const auto result = study.deck()->saveCard("ışık", "", "light", 0);
        QVERIFY(!result.success);
        QVERIFY(!result.message.isEmpty());
        QCOMPARE(study.deck()->cards().size(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);
    }
    void knownMovieHash() {
        QTemporaryDir directory;
        QFile file(directory.filePath("video"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(131072, '\0'));
        file.close();
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
        media.write(QByteArray(131072, '\0'));
        media.close();
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
                if (!bytes->contains("\r\n\r\n") || socket->property("sent").toBool())
                    return;
                socket->setProperty("sent", true);
                const auto target = QString::fromUtf8(bytes->split(' ').value(1));
                requests.append(target);
                if (requests.size() == 1) {
                    socket->write("HTTP/1.1 301 Moved\r\nLocation: " + target.toUtf8() +
                                  "&canonical=1\r\nConnection: close\r\nContent-Length: 0\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                QByteArray body;
                if (target.startsWith("/subtitles?")) {
                    apiAuthenticated &= bytes->toLower().contains("api-key: synthetic-test-key");
                    body =
                        R"({"data":[{"attributes":{"release":"Original fixture","language":"tr","moviehash_match":true,"files":[{"file_id":42}]}}]})";
                } else if (target == "/login") {
                    apiAuthenticated &= bytes->toLower().contains("api-key: synthetic-test-key");
                    body = R"({"token":"synthetic-session-token","base_url":"127.0.0.1"})";
                } else if (target == "/download") {
                    apiAuthenticated &= bytes->toLower().contains("api-key: synthetic-test-key");
                    downloadAuthenticated =
                        bytes->toLower().contains("authorization: bearer synthetic-session-token");
                    body = ("{\"link\":\"" + base + "/file\"}").toUtf8();
                } else {
                    fileUnauthenticated = !bytes->toLower().contains("api-key:") &&
                                          !bytes->toLower().contains("authorization:");
                    body = QString("1\n00:00:01,000 --> 00:00:02,000\nİstanbul ışığı\n").toUtf8();
                }
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        StudyServices study(
            directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr,
            base);
        study.subtitles()->setSubtitleLoginReader(
            [] { return QByteArray(R"({"username":"synthetic-user","password":"synthetic-password"})"); });
        QVector<Cue> received;
        QString savedPath;
        connect(study.subtitles(), &SubtitleService::subtitlesReady, this,
                [&](QVector<Cue> cues, QString path) {
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
        QCOMPARE(parseSrt(QString::fromUtf8(savedSubtitle.readAll())).first().text,
                 QString("İstanbul ışığı"));
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
                if (socket->property("sent").toBool())
                    return;

                socket->setProperty("sent", true);
                requests++;
                socket->write("HTTP/1.1 301 Moved\r\nLocation: /unexpected\r\nConnection: "
                              "close\r\nContent-Length: 0\r\n\r\n");
                socket->disconnectFromHost();
            });
        });
        const auto base = QString("http://127.0.0.1:%1").arg(server.serverPort());
        StudyServices study(
            directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr,
            base);
        study.subtitles()->setMedia(directory.filePath("video"));
        study.subtitles()->findSubtitles("tr", false, "Original show");
        QTRY_VERIFY(study.subtitles()->message().contains("failed"));
        QCOMPARE(requests, 1);
        QVERIFY(study.subtitles()->subtitleResults().isEmpty());
    }
    void incompleteStreams_data() {
        QTest::addColumn<QByteArray>("ending");
        QTest::newRow("missing-done") << QByteArray();
        QTest::newRow("token-limit") << QByteArray(
            "data: {\"choices\":[{\"finish_reason\":\"length\"}]}\n\ndata: [DONE]\n\n");
        QTest::newRow("filtered") << QByteArray(
            "data: {\"choices\":[{\"finish_reason\":\"content_filter\"}]}\n\ndata: [DONE]\n\n");
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
                if (socket->property("sent").toBool())
                    return;
                socket->setProperty("sent", true);
                const auto body =
                    QByteArray("data: {\"choices\":[{\"delta\":{\"content\":\"light\"}}]}\n\n") + ending;
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                              "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        StudyServices study(directory.path(),
                            [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock",
                                            "https://example.org/%s", false));
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
                if (!received->contains("\r\n\r\n") || socket->property("started").toBool())
                    return;
                socket->setProperty("started", true);
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                const auto event =
                    QString("data: {\"choices\":[{\"delta\":{\"content\":\"ışık\"}}]}\n\n").toUtf8();
                const auto split = event.indexOf(QString("ı").toUtf8()) + 1;
                socket->write(event.left(split));
                QTimer::singleShot(40, socket, [socket, event, split] { socket->write(event.mid(split)); });
                QTimer::singleShot(1000, socket, [socket] {
                    socket->write("data: [DONE]\n\n");
                    socket->disconnectFromHost();
                });
            });
        });
        StudyServices study(directory.path(),
                            [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock",
                                            "https://example.org/%s", false));

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
