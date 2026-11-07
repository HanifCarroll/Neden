#include "lookup_history.h"
#include "app_settings.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QUuid>

#include "study_interaction.h"
#include <QTcpServer>
#include <QTcpSocket>

class HistoryPlayback : public PlaybackEngine {
public:
    void open(const QString &, bool) override {}
    void seek(double seconds) override { positionSeconds = seconds; }
    void setPaused(bool paused) override { isPaused = paused; }
    void configure(Option, const QVariant &) override {}
    void addSubtitle(const QString &, bool) override {}
    void rate(double) override {}
    void volume(double) override {}
};
class HistoryServer : public QTcpServer {
public:
    QList<QByteArray> responses;
    int requests = 0;
    HistoryServer() {
        if (!listen(QHostAddress::LocalHost))
            qFatal("Cannot start lookup history fixture");
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                auto request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                if (!request.contains("\r\n\r\n") || socket->property("sent").toBool())
                    return;
                socket->setProperty("sent", true);
                ++requests;
                const auto body = responses.isEmpty() ? QByteArray("{}") : responses.takeFirst();
                socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
    }
    QString origin() const { return QString("http://127.0.0.1:%1").arg(serverPort()); }
    static QByteArray stream(const QString &text) {
        return "data: " +
               QJsonDocument(
                   QJsonObject{{"choices", QJsonArray{QJsonObject{{"delta", QJsonObject{{"content", text}}},
                                                                  {"finish_reason", "stop"}}}}})
                   .toJson(QJsonDocument::Compact) +
               "\n\ndata: [DONE]\n\n";
    }
};

class LookupHistoryTests : public QObject {
    Q_OBJECT
private slots:
    void translationOnlyCompletionDoesNotLogRawJsonAsBreakdown() {
        QTemporaryDir directory;
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        QVERIFY(study.settings()->configureLearning({{"lookupHistoryEnabled", true}}));
        HistoryPlayback playback;
        SubtitleTimeline timeline;
        timeline.replacePrimary({{0, 4000, "Bir ışık", {}}});
        StudyInteraction interaction(playback, timeline);
        interaction.setServices(&study);
        interaction.setMedia(directory.filePath("fixture.webm"));
        interaction.explainCue(); // Missing credentials prevent a real provider request.
        // Exercise the completion boundary independently of today's strict provider parser.
        study.explanation()->responseCompleted("{\"translation\":\"A light.\"}");
        study.history()->flush();
        QFile file(directory.filePath("lookups.jsonl"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        file.readLine();
        const auto result = QJsonDocument::fromJson(file.readLine()).object().value("result").toObject();
        QCOMPARE(result.value("translation").toString(), QString("A light."));
        QCOMPARE(result.value("breakdown").toString(), QString());
    }
    void cancelledWordGestureAndSecondarySelectionKeepRawPrimaryText() {
        QTemporaryDir directory;
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        QVERIFY(study.settings()->configureLearning(
            {{"lookupHistoryEnabled", true}, {"sourceLanguage", "tr"}, {"explanationLanguage", "en"}}));
        HistoryPlayback playback;
        playback.seek(1.5);
        SubtitleTimeline timeline;
        timeline.replacePrimary({{1000, 4000, "[Baba] Bir ışık\nÇığ", {}}});
        QFile secondary(directory.filePath("secondary.srt"));
        QVERIFY(secondary.open(QIODevice::WriteOnly));
        secondary.write("1\n00:00:01,200 --> 00:00:03,900\nA light\n");
        secondary.close();
        timeline.load(QUrl::fromLocalFile(secondary.fileName()), SubtitleRole::Secondary);
        StudyInteraction interaction(playback, timeline);
        interaction.setServices(&study);
        interaction.setMedia(directory.filePath("fixture.webm"));
        interaction.lookupWordAt("A light", 3, true, 1000);
        interaction.closeWordCard(false);
        study.history()->flush();
        QFile file(directory.filePath("lookups.jsonl"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto event = QJsonDocument::fromJson(file.readLine()).object();
        QVERIFY(file.atEnd());
        QCOMPARE(event.value("selection").toString(), QString("light"));
        QCOMPARE(event.value("cueText").toString(), QString("[Baba] Bir ışık\nÇığ"));
        QCOMPARE(event.value("secondaryText").toString(), QString("[Baba] Bir ışık\nÇığ"));
        QCOMPARE(event.value("track").toString(), QString("secondary"));
        QCOMPARE(event.value("sourceLanguage").toString(), QString("auto"));
        QCOMPARE(event.value("cueStartSec").toDouble(), 1.2);
        QCOMPARE(event.value("cueEndSec").toDouble(), 3.9);
        QVERIFY(!event.contains("result"));
    }
    void structuredLineResultsContainTranslationAndMarkdownBreakdown() {
        QTemporaryDir directory;
        HistoryServer server;
        const QJsonObject response{
            {"translation", "A light."},
            {"breakdown", QJsonArray{QJsonObject{{"word", "ışık"}, {"text", "light; a noun"}}}},
            {"alignment", QJsonArray{QJsonObject{{"text", "A light."}, {"source", QJsonArray{"ışık"}}}}}};
        server.responses = {
            HistoryServer::stream(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)))};
        StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
        QVERIFY(study.settings()->configure(server.origin(), "fixture", "https://example.org/%s", false));
        QVERIFY(
            study.settings()->configureLearning({{"lookupHistoryEnabled", true}, {"noKeyRequired", true}}));
        HistoryPlayback playback;
        SubtitleTimeline timeline;
        timeline.replacePrimary({{0, 4000, "Bir ışık", {}}});
        StudyInteraction interaction(playback, timeline);
        interaction.setServices(&study);
        interaction.setMedia(directory.filePath("fixture.webm"));
        interaction.select("ışık");
        interaction.commitSelection();
        QTRY_COMPARE(study.explanation()->state(), QString("complete"));
        study.history()->flush();
        QFile file(directory.filePath("lookups.jsonl"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto gesture = QJsonDocument::fromJson(file.readLine()).object();
        const auto event = QJsonDocument::fromJson(file.readLine()).object();
        QVERIFY(file.atEnd());
        QCOMPARE(event.value("id"), gesture.value("id"));
        QCOMPARE(event.value("kind").toString(), QString("line"));
        QCOMPARE(event.value("selection").toString(), QString("ışık"));
        const auto result = event.value("result").toObject();
        QCOMPARE(result.value("translation").toString(), QString("A light."));
        QVERIFY(result.value("breakdown").toString().contains("**ışık**"));
        QVERIFY(!result.value("breakdown").toString().contains("A light."));
        QVERIFY(!study.explanation()->context().contains("historyCueText"));
    }
    void studyGesturesLogAllKindsWithFrozenContextAndCachedWords() {
        QTemporaryDir directory;
        HistoryServer server;
        const QJsonArray words{QJsonObject{{"word", "Bir"}, {"partOfSpeech", "determiner"}, {"meaning", "a"}},
                               QJsonObject{{"word", "ışık"}, {"partOfSpeech", "noun"}, {"meaning", "light"}}};
        const QJsonObject details{
            {"meaning", "light"}, {"lemma", "ışık"}, {"explanation", "A light."}, {"formNote", ""}};
        server.responses = {
            HistoryServer::stream(QString::fromUtf8(
                QJsonDocument(QJsonObject{{"entries", words}}).toJson(QJsonDocument::Compact))),
            QJsonDocument(
                QJsonObject{
                    {"choices",
                     QJsonArray{QJsonObject{
                         {"finish_reason", "stop"},
                         {"message", QJsonObject{{"content", QString::fromUtf8(QJsonDocument(details).toJson(
                                                                 QJsonDocument::Compact))}}}}}}})
                .toJson(QJsonDocument::Compact),
            HistoryServer::stream("A light."), HistoryServer::stream("Because it is a noun.")};
        {
            StudyServices study(directory.path(), [](const QString &) { return QByteArray(); });
            QVERIFY(study.settings()->configure(server.origin(), "fixture", "https://example.org/%s", false));
            QVERIFY(study.settings()->configureLearning({{"lookupHistoryEnabled", true},
                                                         {"sourceLanguage", "tr"},
                                                         {"explanationLanguage", "en"},
                                                         {"noKeyRequired", true},
                                                         {"includeSecondary", false}}));
            HistoryPlayback playback;
            playback.seek(1.25);
            SubtitleTimeline timeline;
            timeline.replacePrimary({{1000, 4000, "Bir ışık", {}}});
            QFile secondary(directory.filePath("secondary.srt"));
            QVERIFY(secondary.open(QIODevice::WriteOnly));
            secondary.write("1\n00:00:01,000 --> 00:00:04,000\nA light\n");
            secondary.close();
            timeline.load(QUrl::fromLocalFile(secondary.fileName()), SubtitleRole::Secondary);
            StudyInteraction interaction(playback, timeline);
            interaction.setServices(&study);
            const auto media = directory.filePath("fixture.webm");
            interaction.setMedia(media);
            interaction.lookupWordAt("Bir ışık", 1);
            interaction.lookupWordAt("Bir ışık",
                                     5); // Shares the first batch, but each gesture gets a result.
            QTRY_COMPARE(study.words()->state(), QString("complete"));
            QCOMPARE(study.words()->word(), QString("ışık"));
            interaction.lookupWordAt("Bir ışık", 1); // Cache hit, no additional network request.
            QCOMPARE(study.words()->state(), QString("complete"));
            interaction.lookupWordAt("Bir ışık", 5);
            study.words()->loadDetails();
            QTRY_COMPARE(study.words()->detailsState(), QString("complete"));
            study.words()->loadDetails(); // Opening cached Details is also a gesture.
            interaction.explainCue();
            QTRY_COMPARE(study.explanation()->state(), QString("complete"));
            playback.seek(9);
            interaction.setMedia(directory.filePath("another.webm"));
            QVERIFY(interaction.followUp("Why?"));
            QTRY_COMPARE(study.explanation()->state(), QString("complete"));
            QVERIFY(interaction.saveCard().success);
            QCOMPARE(server.requests, 4);
        } // Shutdown drains the disk queue.
        QFile file(directory.filePath("lookups.jsonl"));
        QVERIFY2(file.open(QIODevice::ReadOnly), "Study gestures must produce the opt-in JSONL file");
        QMap<QString, QJsonObject> merged;
        QMap<QString, int> writes;
        while (!file.atEnd()) {
            const auto event = QJsonDocument::fromJson(file.readLine()).object();
            QVERIFY(!event.isEmpty());
            const auto id = event.value("id").toString();
            merged[id] = event;
            ++writes[id];
        }
        QCOMPARE(merged.size(), 9); // Four word clicks, two Details, a line, a follow-up and a save.
        QMap<QString, int> kinds;
        for (auto event = merged.begin(); event != merged.end(); ++event) {
            const auto record = event.value();
            const auto kind = record.value("kind").toString();
            ++kinds[kind];
            QCOMPARE(record.value("media").toString(), directory.filePath("fixture.webm"));
            QCOMPARE(record.value("positionSec").toDouble(), 1.25);
            QCOMPARE(record.value("cueText").toString(), QString("Bir ışık"));
            QCOMPARE(record.value("secondaryText").toString(), QString("A light"));
            QVERIFY(record.contains("result"));
            QCOMPARE(writes[event.key()], 2);
            const auto result = record.value("result").toObject();
            if (kind == "word") {
                QCOMPARE(result.value("word"), record.value("selection"));
                QCOMPARE(result.value("meaning").toString(),
                         record.value("selection") == "Bir" ? QString("a") : QString("light"));
            } else if (kind == "followup") {
                QCOMPARE(result.value("question").toString(), QString("Why?"));
                QCOMPARE(result.value("answer").toString(), QString("Because it is a noun."));
            }
        }
        QCOMPARE(kinds.value("word"), 4);
        QCOMPARE(kinds.value("details"), 2);
        QCOMPARE(kinds.value("line"), 1);
        QCOMPARE(kinds.value("followup"), 1);
        QCOMPARE(kinds.value("save"), 1);
    }
    void allKindsTruncateLineBreakdownAndOmitUnavailableContext() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        QVERIFY(settings.configureLearning({{"lookupHistoryEnabled", true}}));
        LookupHistory history(directory.path(), settings);
        StudyTarget target;
        target.mediaPath = directory.filePath("episode.webm");
        target.selection = "ışık";
        target.historyCueText = "Çığ öş üİ";
        target.role = SubtitleRole::Secondary;
        target.sourceLanguage = "auto";
        target.explanationLanguage = "en";
        const QMap<QString, QJsonObject> results{
            {"word", {{"word", "ışık"}, {"meaning", "light"}}},
            {"details", {{"formNote", "ışık + ı"}, {"otherSenses", QJsonArray{}}}},
            {"line",
             {{"translation", "light"},
              {"breakdown", QString(3999, QChar(u'ş')) + QString::fromUtf8("🌟") + "tail"}}},
            {"followup", {{"question", "Neden?"}, {"answer", "Çünkü."}}},
            {"save", {{"success", true}}}};
        for (auto item = results.begin(); item != results.end(); ++item) {
            const auto event = history.begin(item.key(), target);
            history.complete(event, item.value());
        }
        history.flush();
        QFile file(directory.filePath("lookups.jsonl"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        int count = 0;
        QSet<QString> kinds;
        while (!file.atEnd()) {
            const auto event = QJsonDocument::fromJson(file.readLine()).object();
            QVERIFY(!event.isEmpty());
            QVERIFY(!event.contains("cueStartSec"));
            QVERIFY(!event.contains("cueEndSec"));
            QVERIFY(!event.contains("secondaryText"));
            QVERIFY(!event.contains("offset"));
            QCOMPARE(event.value("track").toString(), QString("secondary"));
            kinds.insert(event.value("kind").toString());
            if (event.value("kind") == "line" && event.contains("result")) {
                const auto result = event.value("result").toObject();
                QCOMPARE(result.value("translation").toString(), QString("light"));
                QCOMPARE(result.value("breakdown").toString(), QString(3999, QChar(u'ş')));
            }
            ++count;
        }
        QCOMPARE(count, 10);
        QCOMPARE(kinds.size(), 5);
    }
    void disabledOrRevokedConsentWritesNothingAndFailuresAreSilent() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        LookupHistory history(directory.filePath("absent"), settings);
        StudyTarget target;
        target.mediaPath = directory.filePath("episode.webm");
        QVERIFY(history.begin("word", target).isEmpty());
        history.complete({}, {{"meaning", "light"}});
        history.flush();
        QVERIFY(!QFile::exists(directory.filePath("absent")));
        QVERIFY(settings.configureLearning({{"lookupHistoryEnabled", true}}));
        const auto event = history.begin("word", target);
        history.flush();
        const auto path = directory.filePath("absent/lookups.jsonl");
        const auto before = QFileInfo(path).size();
        QVERIFY(settings.configureLearning({{"lookupHistoryEnabled", false}}));
        history.complete(event, {{"meaning", "must not be recorded"}});
        history.flush();
        QCOMPARE(QFileInfo(path).size(), before);
        QFile obstruction(directory.filePath("not-a-directory"));
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        obstruction.close();
        QVERIFY(settings.configureLearning({{"lookupHistoryEnabled", true}}));
        LookupHistory failing(obstruction.fileName(), settings);
        QVERIFY(!failing.begin("word", target).isEmpty());
        failing.flush();
        QVERIFY(!QFile::exists(obstruction.fileName() + "/lookups.jsonl"));
    }
    void wordGestureAndResultKeepTheirIdentityAndUnicode() {
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        QVERIFY(settings.configureLearning({{"lookupHistoryEnabled", true}}));
        LookupHistory history(directory.path(), settings);
        StudyTarget target;
        target.mediaPath = directory.filePath("ç ğ ı ö ş ü İ.webm");
        target.selection = "gidiyorsun";
        target.sourceCue = "[Baba] Nereye gidiyorsun? ç ğ ı ö ş ü İ\nİkinci satır";
        target.historyCueText = target.sourceCue;
        target.historySecondaryText = "Where are you going?";
        target.sourceLanguage = "Turkish";
        target.explanationLanguage = "English";
        target.cueIndex = 4;
        target.positionMs = 2871300;
        target.startMs = 2869100;
        target.endMs = 2872400;
        const auto event = history.begin("word", target, 14);
        history.complete(event,
                         {{"word", "gidiyorsun"}, {"partOfSpeech", "verb"}, {"meaning", "you are going"}});
        history.flush();
        QFile file(directory.filePath("lookups.jsonl"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto firstLine = file.readLine();
        const auto first = QJsonDocument::fromJson(firstLine).object();
        const auto second = QJsonDocument::fromJson(file.readLine()).object();
        QVERIFY(file.atEnd());
        QVERIFY(firstLine.contains(QString("ç ğ ı ö ş ü İ").toUtf8()));
        QVERIFY(!first.contains("result"));
        QCOMPARE(first.value("v").toInt(), 1);
        QVERIFY(!QUuid(first.value("id").toString()).isNull());
        const auto timestamp = first.value("at").toString();
        QVERIFY(timestamp.endsWith("Z"));
        QCOMPARE(timestamp.size(), 24);
        QVERIFY(QDateTime::fromString(timestamp, Qt::ISODateWithMs).isValid());
        QCOMPARE(first.value("media").toString(), target.mediaPath);
        QCOMPARE(first.value("positionSec").toDouble(), 2871.3);
        QCOMPARE(first.value("cueStartSec").toDouble(), 2869.1);
        QCOMPARE(first.value("cueEndSec").toDouble(), 2872.4);
        QCOMPARE(first.value("cueText").toString(), target.sourceCue);
        QCOMPARE(first.value("secondaryText").toString(), target.historySecondaryText);
        QCOMPARE(first.value("track").toString(), QString("primary"));
        QCOMPARE(first.value("selection").toString(), target.selection);
        QCOMPARE(first.value("offset").toInt(), 14);
        QCOMPARE(first.value("sourceLanguage").toString(), QString("tr"));
        QCOMPARE(first.value("explanationLanguage").toString(), QString("en"));
        auto merged = second;
        QCOMPARE(merged.take("result").toObject().value("meaning").toString(), QString("you are going"));
        QCOMPARE(merged, first);
        qInfo().noquote() << "Sample JSONL:"
                          << QString::fromUtf8(QJsonDocument(second).toJson(QJsonDocument::Compact));
    }
};
QTEST_GUILESS_MAIN(LookupHistoryTests)
#include "lookup_history_tests.moc"
