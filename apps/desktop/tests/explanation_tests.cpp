#include "explanation_format.h"
#include "explanation_response.h"
#include <QTextDocument>
#include <QtTest>

class ExplanationTests : public QObject {
    Q_OBJECT
private slots:
    void structuredAnswersStreamWithoutShowingJsonAndPreserveText() {
        const QString json =
            "{\"translationHeading\":\"Translation\",\"breakdownHeading\":\"Breakdown\",\"translation\":\"Good luck! Good lu"
            "ck!\",\"breakdown\":[{\"word\":\"Bonne\",\"text\":\"good; feminine adjective.\"},{\"word\":\"chance\",\"text\":\"luck."
            " Together: good luck.\"}],\"alignment\":[{\"text\":\"Good\",\"source\":[\"Bonne\",\"chance\"]},{\"text\":\"luck\",\"so"
            "urce\":[\"chance\"]},{\"text\":\"Good\",\"source\":[\"Bonne\"]},{\"text\":\"luck\",\"source\":[\"chance\"]}]}";
        for (qsizetype size = 1; size <= json.size(); ++size) {
            const auto partial = json.left(size);
            const auto markdown = ExplanationResponse::markdown(partial);
            QVERIFY(!markdown.contains("translationHeading"));
            QVERIFY(!markdown.contains("\"source\""));
            QCOMPARE(ExplanationResponse::isComplete(partial), size == json.size());
        }
        const auto readable = ExplanationResponse::markdown(json, false);
        QTextDocument saved;
        saved.setMarkdown(readable);
        QVERIFY(saved.toPlainText().contains("Good luck! Good luck!"));
        QVERIFY(!readable.contains("neden-breakdown:"));
        const auto html = ExplanationFormat::render(json);
        QVERIFY(html.contains("href="));
        QVERIFY(ExplanationFormat::breakdown(json, "neden-breakdown:Bonne,chance").contains("feminine"));
        QTextDocument document;
        document.setHtml(html);
        QVERIFY(document.toPlainText().contains("Good luck! Good luck!"));
    }
    void structuredTextEscapesMarkupAndKeepsInvalidMappingsReadable() {
        const QString json =
            "{\"translation\":\"A [word] <image>\",\"breakdown\":[{\"word\":\"mot\",\"text\":\"A \\\"word\\\" with a brace } and ["
            "brackets].\"}],\"alignment\":[{\"text\":\"missing\",\"source\":[\"mot\"]},{\"text\":\"word\",\"source\":[\"nonexistent"
            "\"]}]}";
        QTextDocument document;
        const auto html = ExplanationFormat::render(json);
        document.setHtml(html);
        QVERIFY(document.toPlainText().contains("A [word] <image>"));
        QVERIFY(document.toPlainText().contains("brace }"));
        QVERIFY(!html.contains("href="));
        QVERIFY(!ExplanationResponse::isComplete("{\"translation\":\"x\"}"));
    }
    void translationReferencesResolveSourceEntries() {
        const QString markdown = "### Traduction\n[Good](neden-breakdown:Bonne,chance) [luck](neden-breakdown:Bonne,chance)!\n\n"
            "### Détail\n- **Bonne** — good; feminine adjective.\n- **chance** — luck. Together: good luck.\n";
        const auto entry = ExplanationFormat::breakdown(markdown, "neden-breakdown:Bonne,chance");
        QVERIFY(entry.startsWith("Bonne"));
        QVERIFY(entry.contains("chance"));
        QVERIFY(ExplanationFormat::render(markdown).contains("href=\"neden-breakdown:Bonne,chance\""));
        QVERIFY(ExplanationFormat::breakdown(markdown, "neden-breakdown:Bonne").isEmpty());
    }
    void invalidAndExternalReferencesRemainPlainText() {
        for (const auto &reference : {"neden-breakdown:0", "neden-breakdown:9", "neden-breakdown:1,9",
             "neden-breakdown:-1", "neden-breakdown:1?x=2", "https://example.org", "javascript:alert(1)"}) {
            const QString markdown = QString("### Translation\n[word](%1)\n### Breakdown\n- **word** — meaning.").arg(reference);
            QVERIFY(ExplanationFormat::breakdown(markdown, reference).isEmpty());
            QVERIFY(!ExplanationFormat::render(markdown).contains("href="));
        }
        const QString outside = "### Translation\nPlain\n### Breakdown\n- [**word**](neden-breakdown:word) — meaning.";
        QVERIFY(!ExplanationFormat::render(outside).contains("href="));
        QVERIFY(ExplanationFormat::breakdown(outside, "neden-breakdown:word").isEmpty());
    }
    void mappingsHandleStreamingRepeatedWordsAndOtherScripts() {
        const QString first = "### 翻訳\n[こんにちは](neden-breakdown:مرحبا)\n### 解説\n- **أهلا** — first greeting.\n";
        QVERIFY(!ExplanationFormat::render(first).contains("href="));
        const QString complete = first + "- **مرحبا** — repeated greeting.\n";
        QCOMPARE(ExplanationFormat::breakdown(complete, "neden-breakdown:مرحبا"), QString("مرحبا — repeated greeting."));
        QVERIFY(ExplanationFormat::render(complete).contains("href="));
        const QString nested = "### Translation\n[here](neden-breakdown:two)\n### Breakdown\n- **one** — first\n    - example\n- **two** — second\n";
        QVERIFY(ExplanationFormat::breakdown(nested, "neden-breakdown:two").startsWith("two"));
    }
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
        for (const auto &input :
             {QString("### Breakdown\n\n- **ış"), QString("### Breakdown\n\n- **ışık**: light")}) {
            QTextDocument document;
            document.setHtml(ExplanationFormat::render(input));
            QVERIFY(document.toPlainText().contains("ış"));
        }
    }
};
QTEST_MAIN(ExplanationTests)
#include "explanation_tests.moc"
