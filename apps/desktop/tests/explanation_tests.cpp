#include "explanation_format.h"
#include <QTextDocument>
#include <QtTest>

class ExplanationTests : public QObject {
    Q_OBJECT
private slots:
    void formattingAndUntrustedResources() {
        const auto html = ExplanationFormat::render(
            "### Natural meaning\n\n**İstanbul** and *ışık*.\n\n- çığ\n- 日本語\n\n"
            "[label](javascript:alert) ![image label](https://example.invalid/track.png)\n\n"
            "<img src=\"file:///private/secret\"> <script>unsafe</script>");
        QVERIFY(!html.contains("<img"));
        QVERIFY(!html.contains("href="));
        QVERIFY(!html.contains("<script>"));
        QVERIFY(!html.contains("src=\"file:///private/secret\""));
        QTextDocument document;
        document.setHtml(html);
        const auto text = document.toPlainText();
        QVERIFY(text.contains("Natural meaning"));
        QVERIFY(text.contains("İstanbul"));
        QVERIFY(text.contains("ışık"));
        QVERIFY(text.contains("日本語"));
        QVERIFY(text.contains("image label"));
        QVERIFY(!text.contains("###"));
        QVERIFY(!text.contains("**"));
    }
    void partialStreamRemainsReadable() {
        for (const auto &input : {QString("### Breakdown\n\n- **ış"), QString("### Breakdown\n\n- **ışık**: light")}) {
            QTextDocument document;
            document.setHtml(ExplanationFormat::render(input));
            QVERIFY(document.toPlainText().contains("ış"));
        }
    }
};
QTEST_MAIN(ExplanationTests)
#include "explanation_tests.moc"
