#include "study_tools.h"
#include "subtitle_appearance.h"
#include "network_error.h"
#include <QTcpServer>
#include <QUrlQuery>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QFile>
#include <QTimer>
#include <QtTest>

class StudyTests : public QObject {
    Q_OBJECT
private slots:
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
        StudyTools study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock", "https://example.org/%s", false));
        study.explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE(study.state(), QString("failed"));
        QVERIFY(study.message().contains("allowance"));
        QVERIFY(!study.message().contains("private provider details"));
        QVERIFY(!study.message().contains("HTTP 0"));
        QTest::qWait(200);
        QCOMPARE(requests, 1);
        study.retryExplanation();
        QTRY_COMPARE(study.state(), QString("complete"));
        QCOMPARE(requests, 2);
        QCOMPARE(study.subject(), QString("ışık"));
        QCOMPARE(study.answer(), QString("Light"));
    }
    void dictionaryEncodingAndDestinationValidation() {
        const auto url = StudyTools::buildDictionaryUrl("https://example.org/?q=%s", "İstanbul & ışık?#");
        QCOMPARE(QUrlQuery(url).queryItemValue("q", QUrl::FullyDecoded), QString("İstanbul & ışık?#"));
        QVERIFY(StudyTools::buildDictionaryUrl("https://%s.example.org", "evil").isEmpty());
        QVERIFY(StudyTools::buildDictionaryUrl("javascript:%s", "test").isEmpty());
        QVERIFY(StudyTools::buildDictionaryUrl("https://user:pass@example.org/%s", "test").isEmpty());
    }
    void persistentCardsAndLiteralExport() {
        QTemporaryDir directory;
        StudyTools study(directory.path(), [](const QString &) { return QByteArray(); });
        study.saveCard("ışık", "<script>context</script>\nline", "light\tbright", 1.5);
        QCOMPARE(study.cards().size(), 1);
        study.exportCards(QUrl::fromLocalFile(directory.filePath("cards.tsv")));
        QFile exportFile(directory.filePath("cards.tsv"));
        QVERIFY(exportFile.open(QIODevice::ReadOnly));
        const auto text = exportFile.readAll();
        QVERIFY(text.contains("#html:false"));
        QVERIFY(text.contains("light bright"));
        StudyTools restored(directory.path(), [](const QString &) { return QByteArray(); });
        QCOMPARE(restored.cards().size(), 1);
        restored.reviewCard(0, true);
        QCOMPARE(restored.cards().first().toMap().value("reviews").toInt(), 1);
        restored.removeCard(0);
        QCOMPARE(restored.cards().size(), 0);
    }
    void corruptStorageIsPreserved() {
        QTemporaryDir directory;
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("broken data"); file.close();
        StudyTools study(directory.path(), [](const QString &) { return QByteArray(); });
        study.saveCard("ışık", "", "light", 0);
        QCOMPARE(study.cards().size(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("broken data"));
    }
    void invalidCardSchemaIsPreserved() {
        QTemporaryDir directory;
        QFile file(directory.filePath("study.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray data = R"({"version":1,"cards":[{"front":"ışık"}]})";
        file.write(data); file.close();
        StudyTools study(directory.path(), [](const QString &) { return QByteArray(); });
        study.saveCard("ışık", "", "light", 0);
        QCOMPARE(study.cards().size(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);
    }
    void knownMovieHash() {
        QTemporaryDir directory;
        QFile file(directory.filePath("video"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(131072, '\0')); file.close();
        QCOMPARE(StudyTools::movieHash(file.fileName()), QString("0000000000020000"));
    }
    void missingCredentialSendsNothing() {
        QTemporaryDir directory;
        StudyTools study(directory.path(), [](const QString &) { return QByteArray(); });
        study.explain("ışık", "Bir ışık gördüm.");
        QCOMPARE(study.state(), QString("failed"));
        QVERIFY(study.answer().isEmpty());
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
        StudyTools study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr, base);
        study.setSubtitleLoginReader([] { return QByteArray(R"({"username":"synthetic-user","password":"synthetic-password"})"); });
        QVector<Cue> received;
        connect(&study, &StudyTools::subtitlesReady, this, [&](QVector<Cue> cues) { received = cues; });

        // 2. An exact match may auto-download, but signed file requests never receive the key.
        study.setMedia(media.fileName());
        study.findSubtitles("tr", true);
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(received.first().text, QString("İstanbul ışığı"));
        QCOMPARE(requests.size(), 5);
        QVERIFY(requests.first().contains("languages=tr&moviehash=0000000000020000"));
        QVERIFY(apiAuthenticated);
        QVERIFY(fileUnauthenticated);
        QVERIFY(downloadAuthenticated);

        // 3. A manual title/episode search does not inherit the movie hash or auto-download.
        study.findSubtitles("tr", false, "Original show", 2, 1);
        QTRY_COMPARE(requests.size(), 6);
        const QUrlQuery query(QUrl("http://localhost" + requests.last()));
        QCOMPARE(query.queryItemValue("query"), QString("Original show"));
        QCOMPARE(query.queryItemValue("season_number"), QString("2"));
        QCOMPARE(query.queryItemValue("episode_number"), QString("1"));
        QVERIFY(!query.hasQueryItem("moviehash"));
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
        StudyTools study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); }, nullptr, base);
        study.setMedia(directory.filePath("video"));
        study.findSubtitles("tr", false, "Original show");
        QTRY_VERIFY(study.message().contains("failed"));
        QCOMPARE(requests, 1);
        QVERIFY(study.subtitleResults().isEmpty());
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
        StudyTools study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock", "https://example.org/%s", false));
        study.explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE(study.state(), QString("incomplete"));
        QCOMPARE(study.answer(), QString("light"));
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
        StudyTools study(directory.path(), [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.configure(QString("http://127.0.0.1:%1").arg(server.serverPort()), "mock", "https://example.org/%s", false));

        // 2. Prove visible text precedes completion, then cancel and reject late output.
        study.explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE_WITH_TIMEOUT(study.answer(), QString("ışık"), 500);
        QCOMPARE(study.state(), QString("streaming"));
        study.stop();
        QCOMPARE(study.state(), QString("incomplete"));
        QTRY_VERIFY(disconnected);
        study.invalidateContext();
        QTest::qWait(1100);
        QVERIFY(study.answer().isEmpty());

        // 3. Require a successful end marker on a new request.
        study.explain("ışık", "Bir ışık gördüm.");
        QTRY_COMPARE_WITH_TIMEOUT(study.state(), QString("complete"), 2000);
        QCOMPARE(study.answer(), QString("ışık"));
    }
};
QTEST_GUILESS_MAIN(StudyTests)
#include "study_tests.moc"
