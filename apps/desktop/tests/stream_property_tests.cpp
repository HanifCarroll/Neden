#include "explanation_stream.h"
#include "property_support.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class StreamPropertyTests : public QObject {
    Q_OBJECT
    static QByteArray delta(const QString &text) {
        return "data: " +
               QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{
                                                         {"delta", QJsonObject{{"content", text}}}}}}})
                   .toJson(QJsonDocument::Compact) +
               "\r\n\r\n";
    }
private slots:
    void arbitraryBytePartitionsPreserveText_data() { PropertySupport::seeds(); }
    void arbitraryBytePartitionsPreserveText() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        QString expected;
        QByteArray wire = ": heartbeat\r\n\r\n";
        for (int count = PropertySupport::number(random, 1, 12); count > 0; --count) {
            const auto text = PropertySupport::text(random);
            expected += text;
            wire += delta(text);
        }
        wire += "data: [DONE]\r\n\r\n";
        ExplanationStream whole, fragmented;
        QCOMPARE(whole.append(wire).join(""), expected);
        QString received;
        for (qsizetype offset = 0; offset < wire.size();) {
            const auto count = PropertySupport::number(random, 1, 41);
            received += fragmented.append(wire.mid(offset, count)).join("");
            offset += count;
        }
        QCOMPARE(received, expected);
        QVERIFY(fragmented.complete());
        QVERIFY(fragmented.error().isEmpty());
        QVERIFY(whole.complete());
        // Every cut before the complete terminal line must remain incomplete.
        const int cut = PropertySupport::number(random, 0, wire.indexOf("data: [DONE]") + 10);
        ExplanationStream interrupted;
        interrupted.append(wire.left(cut));
        QVERIFY(!interrupted.complete());
    }

    void everySingleByteBoundaryPreservesUnicode() {
        const QString text = "İı日本語🙂 cafe\u0301\n\"escaped\"";
        const auto wire = delta(text) + "data: [DONE]\n\n";
        for (qsizetype split = 0; split <= wire.size(); ++split) {
            ExplanationStream stream;
            const auto first = stream.append(wire.left(split));
            const auto second = stream.append(wire.mid(split));
            QCOMPARE((first + second).join(""), text);
            QVERIFY(stream.complete());
        }
    }

    void malformedOrTruncatedStreamsNeverComplete_data() { PropertySupport::seeds(); }
    void malformedOrTruncatedStreamsNeverComplete() {
        QFETCH(uint, seed);
        std::mt19937 random(seed);
        const QByteArray prefix = delta(PropertySupport::text(random));
        const QList<QByteArray> endings{
            "",
            "data: {",
            "data: []\n\n",
            "data: {\"error\":{}}\n\n",
            "data: {\"choices\":[{\"finish_reason\":\"length\"}]}\n\ndata: [DONE]\n\n",
            "data: {\"choices\":[{\"finish_reason\":\"content_filter\"}]}\n\ndata: [DONE]\n\n",
            "data: [DONE]\n\ndata: {",
            "data: [DONE]\n\n" + delta("late")};
        for (const auto &ending : endings) {
            ExplanationStream stream;
            const auto wire = prefix + ending;
            for (qsizetype offset = 0; offset < wire.size();) {
                const int size = PropertySupport::number(random, 1, 23);
                stream.append(wire.mid(offset, size));
                offset += size;
            }
            QVERIFY(!stream.complete());
        }
    }

    void oversizedFramesFailAndCannotRecoverAsSuccess() {
        ExplanationStream stream;
        const QByteArray block(64 * 1024, 'x');
        for (int count = 0; count <= 128; ++count)
            stream.append(block);
        QVERIFY(!stream.error().isEmpty());
        QVERIFY(stream.append("\ndata: [DONE]\n\n").isEmpty());
        QVERIFY(!stream.complete());
    }
};
QTEST_GUILESS_MAIN(StreamPropertyTests)
#include "stream_property_tests.moc"
