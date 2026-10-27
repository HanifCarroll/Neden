#include "subtitle_document.h"
#include "subtitle_tools.h"
#include "subtitle_timeline.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <random>

class TranslationServer : public QTcpServer {
public:
    QVector<QJsonObject> requests;
    bool incomplete = false, hold = false;
    TranslationServer() {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = nextPendingConnection()) {
                auto buffer = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
                    buffer->append(socket->readAll());
                    const int headerEnd = buffer->indexOf("\r\n\r\n");
                    if (headerEnd < 0)
                        return;
                    int length = -1;
                    for (const auto &line : buffer->left(headerEnd).split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toInt();
                    if (length < 0 || buffer->size() < headerEnd + 4 + length)
                        return;
                    socket->disconnect(this);
                    const auto body = QJsonDocument::fromJson(buffer->mid(headerEnd + 4)).object();
                    requests.append(body);
                    if (hold)
                        return;
                    const auto data =
                        QJsonDocument::fromJson(
                            body["messages"].toArray()[1].toObject()["content"].toString().toUtf8())
                            .object();
                    auto cues = data["cues"].toArray();
                    QJsonArray translated;
                    for (const auto &cue : cues)
                        translated.append(
                            QJsonObject{{"id", cue.toObject()["id"]},
                                        {"text", "Translated " + cue.toObject()["text"].toString()}});
                    if (incomplete)
                        translated.removeLast();
                    const QJsonObject answer{{"cues", translated}};
                    const auto content =
                        QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact));
                    const auto response =
                        QJsonDocument(
                            QJsonObject{{"choices", QJsonArray{QJsonObject{
                                                        {"finish_reason", "stop"},
                                                        {"message", QJsonObject{{"content", content}}}}}}})
                            .toJson(QJsonDocument::Compact);
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                                  QByteArray::number(response.size()) + "\r\nConnection: close\r\n\r\n" +
                                  response);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QString endpoint() const { return QString("http://127.0.0.1:%1").arg(serverPort()); }
};
class SubtitleToolsTests : public QObject {
    Q_OBJECT
    static QVector<Cue> track(int count = 32) {
        QVector<Cue> cues;
        std::mt19937 random(17);
        qint64 start = 30000;
        for (int i = 0; i < count; ++i) {
            const int duration = 500 + random() % 3500;
            cues.append({start, start + duration, QString("Original line %1").arg(i), {}});
            start += duration + 1000 + random() % 8000;
        }
        return cues;
    }
private slots:
    void savesSeparateRoundTrippableCopies() {
        QTemporaryDir directory;
        const auto original = track();
        const auto first = SubtitleDocument::save(original, directory.path(), "../../English");
        const auto second = SubtitleDocument::save(original, directory.path(), "English");
        QVERIFY(first != second);
        QVERIFY(first.startsWith(directory.path() + "/subtitles/generated/"));
        const auto loaded = SubtitleTimeline::read(QUrl::fromLocalFile(first));
        QCOMPARE(loaded.size(), original.size());
        for (int i = 0; i < original.size(); ++i) {
            QCOMPARE(loaded[i].startMs, original[i].startMs);
            QCOMPARE(loaded[i].endMs, original[i].endMs);
            QCOMPARE(loaded[i].text, original[i].text);
        }
    }
    void anchorsCorrectOffsetAndDriftWithoutDroppingWords() {
        const auto source = track();
        const auto timing = SubtitleDocument::fromAnchors(30000, 32000, 130000, 136000);
        QCOMPARE(timing.rate, 1.04);
        const auto repaired = SubtitleDocument::retime(source, timing);
        QCOMPARE(repaired.first().startMs, 32000LL);
        for (int i = 0; i < source.size(); ++i)
            QCOMPARE(repaired[i].text, source[i].text);
        QVERIFY_EXCEPTION_THROWN(SubtitleDocument::fromAnchors(30000, 2000, 31000, 9000), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(SubtitleDocument::retime(source, {1, -900000, 1}), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(SubtitleDocument::retime(source, {NAN, 0, 1}), std::runtime_error);
    }
    void automaticAlignmentFindsOffsetAndFrameRateDrift() {
        const auto source = track(250);
        const auto accurate = SubtitleDocument::retime(source, {25.0 / 24, 4350, 1});
        std::atomic_bool cancelled{false};
        const auto match = SubtitleDocument::align(source, accurate, cancelled);
        QVERIFY(std::abs(match.offsetMs - 4350) <= 100);
        QVERIFY(std::abs(match.rate - 25.0 / 24) < 0.0001);
        QVERIFY(match.confidence > 0.9);
        cancelled = true;
        QVERIFY_EXCEPTION_THROWN(SubtitleDocument::align(source, accurate, cancelled), std::runtime_error);
    }
    void ambiguousAlignmentFailsInsteadOfChangingTrack() {
        QVector<Cue> repeated;
        for (int i = 0; i < 100; ++i)
            repeated.append({300000 + i * 5000, 302000 + i * 5000, "Repeated", {}});
        std::atomic_bool cancelled{false};
        QVERIFY_EXCEPTION_THROWN(SubtitleDocument::align(repeated, repeated, cancelled), std::runtime_error);
    }
    void translationBatchesPreserveEveryTimestamp() {
        TranslationServer server;
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        QVERIFY(settings.configure(server.endpoint(), "test", "https://example.com/%s", false));
        SubtitleTools tools(directory.path(), settings, [](const QString &) { return QByteArray("test"); });
        const auto original = track(19);
        tools.prepare(original, {}, false);
        QCOMPARE(tools.translationBatches(), 3);
        QSignalSpy published(&tools, &SubtitleTools::trackReady);
        tools.translate("Spanish");
        QTRY_COMPARE_WITH_TIMEOUT(published.count(), 1, 5000);
        QCOMPARE(server.requests.size(), 3);
        QVERIFY(published.first()[1].toBool());
        const auto result = SubtitleTimeline::read(QUrl::fromLocalFile(published.first()[0].toString()));
        QCOMPARE(result.size(), original.size());
        for (int i = 0; i < result.size(); ++i) {
            QCOMPARE(result[i].startMs, original[i].startMs);
            QCOMPARE(result[i].endMs, original[i].endMs);
            QCOMPARE(result[i].text, "Translated " + original[i].text);
        }
        const auto second =
            QJsonDocument::fromJson(
                server.requests[1]["messages"].toArray()[1].toObject()["content"].toString().toUtf8())
                .object();
        QCOMPARE(second["contextBefore"].toArray().size(), 2);
        QVERIFY(!tools.busy());
    }
    void failedOrCancelledTranslationNeverPublishesPartialFiles() {
        TranslationServer server;
        server.incomplete = true;
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        QVERIFY(settings.configure(server.endpoint(), "test", "https://example.com/%s", false));
        SubtitleTools tools(directory.path(), settings, [](const QString &) { return QByteArray("test"); });
        tools.prepare(track(), {}, false);
        QSignalSpy published(&tools, &SubtitleTools::trackReady);
        tools.translate("English");
        QTRY_VERIFY(!tools.busy());
        QCOMPARE(published.size(), 0);
        QVERIFY(tools.message().contains("omitted"));
        QVERIFY(!QDir(directory.path() + "/subtitles/generated").exists());
        server.hold = true;
        tools.translate("English");
        QTRY_COMPARE(server.requests.size(), 2);
        tools.invalidate();
        QVERIFY(!tools.busy());
        QCOMPARE(published.size(), 0);
        QCOMPARE(tools.lineCount(), 0);
    }
    void rejectsInventedLanguagesBeforeAnyRequest() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        SubtitleTranslation translation(settings, [](const QString &) { return QByteArray(); });
        QSignalSpy failed(&translation, &SubtitleTranslation::failed);
        translation.start(track(), "invented language");
        QCOMPARE(failed.size(), 1);
    }
    void cancellationRejectsLateExtractionAndAllowsTwoRoles() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        SubtitleTools tools(directory.path(), settings, [](const QString &) { return QByteArray(); });
        tools.setDecoder([](const QString &, int, const std::atomic_bool &cancelled) {
            while (!cancelled)
                QThread::msleep(1);
            return track();
        });
        QSignalSpy published(&tools, &SubtitleTools::trackReady);
        tools.extract("video", 1, false);
        tools.invalidate();
        QTest::qWait(30);
        QCOMPARE(published.size(), 0);
        tools.setDecoder([](const QString &, int, const std::atomic_bool &) { return track(); });
        tools.extract("video", 1, false);
        tools.extract("video", 2, true);
        QTRY_COMPARE(published.size(), 2);
    }
    void repairRequiresExplicitApply() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        SubtitleTools tools(directory.path(), settings, [](const QString &) { return QByteArray(); });
        tools.prepare(track(), {}, false);
        QSignalSpy published(&tools, &SubtitleTools::trackReady);
        tools.matchLines(0, 32);
        QVERIFY(tools.hasRepair());
        QCOMPARE(published.size(), 0);
        tools.applyRepair();
        QCOMPARE(published.size(), 1);
        QVERIFY(!published.first()[1].toBool());
        QCOMPARE(SubtitleTimeline::read(QUrl::fromLocalFile(published.first()[0].toString())).first().startMs,
                 32000LL);
    }
};
QTEST_GUILESS_MAIN(SubtitleToolsTests)
#include "subtitle_tools_tests.moc"
