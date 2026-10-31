#include "learning_session.h"
#include "property_support.h"
#include <QFile>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

class PropertyPlayback : public PlaybackEngine {
public:
    PropertyPlayback() {
        durationSeconds = 100;
        canSeek = true;
    }
    void open(const QString &path, bool) override {
        seek(0);
        emit loaded(path);
    }
    void seek(double seconds) override {
        positionSeconds = seconds;
        emit positionChanged();
    }
    void setPaused(bool value) override {
        isPaused = value;
        emit pausedChanged();
    }
    void rate(double) override {}
    void volume(double) override {}
};

// Responses remain under test control after the entire HTTP request has arrived.
class HeldExplanationServer : public QTcpServer {
public:
    QVector<QPointer<QTcpSocket>> replies;
    HeldExplanationServer() {
        if (!listen(QHostAddress::LocalHost))
            qFatal("Cannot listen on loopback");
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    if (socket->property("received").toBool())
                        return;
                    const auto request = socket->property("request").toByteArray() + socket->readAll();
                    socket->setProperty("request", request);
                    const auto end = request.indexOf("\r\n\r\n");
                    if (end < 0)
                        return;
                    int length = -1;
                    for (const auto &line : request.left(end).split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toInt();
                    if (length < 0 || request.size() < end + 4 + length)
                        return;
                    socket->setProperty("received", true);
                    replies.append(socket);
                    socket->write(
                        "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                });
            }
        });
    }
    QString endpoint() const { return QString("http://127.0.0.1:%1").arg(serverPort()); }
    void deliver(int index, const QString &text, bool finish) {
        if (index < 0 || !replies[index] || replies[index]->state() != QAbstractSocket::ConnectedState)
            return;
        auto *socket = replies[index].data();
        const auto event =
            QJsonDocument(
                QJsonObject{{"choices", QJsonArray{QJsonObject{{"delta", QJsonObject{{"content", text}}}}}}})
                .toJson(QJsonDocument::Compact);
        socket->write("data: " + event + "\n\n");
        if (finish) {
            socket->write("data: [DONE]\n\n");
            socket->disconnectFromHost();
        }
    }
};

class SessionPropertyTests : public QObject {
    Q_OBJECT
private slots:
    void cancellationDuringDeliveryDiscardsAlreadyDecodedDeltas() {
        QTemporaryDir directory;
        HeldExplanationServer server;
        StudyServices study(directory.path(),
                            [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(server.endpoint(), "mock", "https://example.org/%s", false));
        auto *explanation = study.explanation();
        connect(explanation, &ExplanationSession::changed, this, [explanation] {
            if (explanation->state() == "streaming")
                explanation->stop();
        });
        explanation->explain("word", "source cue");
        QTRY_COMPARE_WITH_TIMEOUT(server.replies.size(), 1, 2000);
        server.replies.first()->write(
            "data: {\"choices\":[{\"delta\":{\"content\":\"first\"}}]}\n\n"
            "data: {\"choices\":[{\"delta\":{\"content\":\"STALE\"}}]}\n\ndata: [DONE]\n\n");
        QTRY_COMPARE_WITH_TIMEOUT(explanation->state(), QString("incomplete"), 2000);
        QCOMPARE(explanation->answer(), QString("first"));
        QTest::qWait(20);
        QCOMPARE(explanation->answer(), QString("first"));
        QCOMPARE(explanation->state(), QString("incomplete"));
    }
    void generatedActionsRejectStaleRepliesAndKeepSavedTargets_data() { PropertySupport::seeds(16); }
    void generatedActionsRejectStaleRepliesAndKeepSavedTargets() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        QTemporaryDir directory;
        HeldExplanationServer server;
        StudyServices study(directory.path(),
                            [](const QString &) { return QByteArray("synthetic-test-key"); });
        QVERIFY(study.settings()->configure(server.endpoint(), "mock", "https://example.org/%s", false));
        PropertyPlayback playback;
        LearningSession session(&playback);
        session.setStudyServices(&study);
        QFile video(directory.filePath("synthetic.mkv"));
        QVERIFY(video.open(QIODevice::WriteOnly));
        video.close();
        session.openVideo(QUrl::fromLocalFile(video.fileName()));
        const QVector<Cue> cues{{0, 10000, "Bir ışık gördüm.", {}},
                                {10000, 20000, "日本語の字幕", {}},
                                {20000, 30000, "مرحبا بالعالم", {}}};
        session.replaceCues(cues);
        auto *explanation = study.explanation();
        int active = -1;
        QString expectedAnswer, expectedSubject, expectedState = "idle";
        double expectedPosition = 0;
        QStringList trace;
        for (int step = 0; step < 40; ++step) {
            // Always exercise a complete/save sequence, then explore arbitrary interleavings.
            const int command = step < 3 ? step : PropertySupport::number(random, 0, 7);
            trace.append(QString::number(command));
            const auto context = QString("seed=%1 commands=%2").arg(seed).arg(trace.join(','));
            switch (command) {
            case 0: {
                if (active >= 0)
                    server.deliver(active, "STALE", true); // Queued before the old request is cancelled.
                const int cue = PropertySupport::number(random, 0, cues.size() - 1);
                expectedPosition = cue * 10 + PropertySupport::number(random, 1, 9000) / 1000.0;
                session.seek(expectedPosition);
                session.select(cues[cue].text);
                const int count = server.replies.size();
                session.explainSelection();
                QTRY_COMPARE_WITH_TIMEOUT(server.replies.size(), count + 1, 2000);
                active = count;
                expectedSubject = cues[cue].text;
                expectedAnswer.clear();
                expectedState = "loading";
                break;
            }
            case 1:
                if (active >= 0) {
                    const auto answer = QString("Answer %1").arg(step);
                    server.deliver(active, answer, true);
                    expectedAnswer += answer;
                    expectedState = "complete";
                    QTRY_COMPARE_WITH_TIMEOUT(explanation->state(), expectedState, 2000);
                    active = -1;
                }
                break;
            case 2:
                // Saving after unrelated playback movement must use the original frozen target.
                session.seek(PropertySupport::number(random, 31, 99));
                if (active >= 0) {
                    active = -1;
                    expectedState = expectedAnswer.isEmpty() ? "failed" : "incomplete";
                }
                if (expectedState == "complete") {
                    const int count = study.deck()->cards().size();
                    session.saveCard();
                    QCOMPARE(study.deck()->cards().size(), count + 1);
                    const auto card = study.deck()->cards().last().toMap();
                    QCOMPARE(card.value("front").toString(), expectedSubject);
                    QCOMPARE(card.value("back").toString(), expectedAnswer);
                    QCOMPARE(card.value("context").toString(), expectedSubject);
                    QVERIFY(std::abs(card.value("seconds").toDouble() - expectedPosition) < 0.001);
                    QCOMPARE(card.value("media").toString(), QString("synthetic.mkv"));
                }
                break;
            case 3:
                if (active >= 0)
                    server.deliver(active, "STALE", true);
                explanation->stop();
                if (active >= 0)
                    expectedState = expectedAnswer.isEmpty() ? "failed" : "incomplete";
                active = -1;
                break;
            case 4:
                if (active >= 0)
                    server.deliver(active, "STALE", true);
                session.replaceCues(cues); // Track replacement invalidates the entire context.
                active = -1;
                expectedState = "idle";
                expectedAnswer.clear();
                expectedSubject.clear();
                QVERIFY(!explanation->target());
                break;
            case 5:
                // Send callbacks for every retired request, including after cancellation/replacement.
                for (int index = 0; index < server.replies.size(); ++index)
                    if (index != active)
                        server.deliver(index, "STALE", true);
                QTest::qWait(1);
                break;
            case 6:
                if (active >= 0) {
                    const auto part = QString("Part %1 ").arg(step);
                    server.deliver(active, part, false);
                    expectedAnswer += part;
                    expectedState = "streaming";
                    QTRY_COMPARE_WITH_TIMEOUT(explanation->answer(), expectedAnswer, 2000);
                }
                break;
            case 7:
                if (expectedState == "failed" || expectedState == "incomplete") {
                    const int count = server.replies.size();
                    explanation->retryExplanation();
                    QTRY_COMPARE_WITH_TIMEOUT(server.replies.size(), count + 1, 2000);
                    active = count;
                    expectedAnswer.clear();
                    expectedState = "loading";
                }
                break;
            }
            QVERIFY2(explanation->state() == expectedState,
                     qPrintable(context + " state=" + explanation->state()));
            QVERIFY2(explanation->answer() == expectedAnswer, qPrintable(context));
            QCOMPARE(explanation->subject(), expectedSubject);
            if (explanation->target()) {
                QCOMPARE(explanation->target()->selection, expectedSubject);
                QVERIFY(std::abs(explanation->target()->positionMs / 1000.0 - expectedPosition) < 0.001);
            }
        }
        explanation->stop();
    }
};
QTEST_GUILESS_MAIN(SessionPropertyTests)
#include "session_property_tests.moc"
