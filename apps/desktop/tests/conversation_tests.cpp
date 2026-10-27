#include "conversation.h"
#include <QJsonDocument>
#include <QtTest>

class ConversationTests : public QObject {
    Q_OBJECT
private slots:
    void languageContextSurvivesAcrossDifferentWritingSystems() {
        for (const auto &context : QList<QJsonObject>{
                 {{"selection", "你好"}, {"sourceLanguage", "Chinese"}, {"explanationLanguage", "Arabic"}},
                 {{"selection", "مرحبًا"}, {"sourceLanguage", "Arabic"}, {"explanationLanguage", "French"}},
                 {{"selection", "こんにちは"}, {"sourceLanguage", "Japanese"}, {"explanationLanguage", "Spanish"}}}) {
            Conversation chat;
            chat.begin(context);
            const auto sent = QJsonDocument::fromJson(chat.messages()[1].toObject()["content"].toString().toUtf8());
            QCOMPARE(sent.object(), context);
        }
    }
    void followUpsKeepFrozenContextAndCompletedHistory() {
        Conversation chat;
        QJsonObject context{{"selection", "dalmışım"},
                            {"sourceCue", "Ben televizyona dalmışım ya."},
                            {"before", QJsonArray{"Önceki cümle."}},
                            {"secondary", QJsonArray{"I was watching TV."}}};
        chat.begin(context);
        chat.append("### Natural meaning\nI was absorbed in it.");
        chat.finish("complete");
        context.insert("selection", "changed elsewhere");
        chat.followUp("Why use -mış here?");
        const auto messages = chat.messages();
        QCOMPARE(messages.size(), 4);
        QVERIFY(messages[1].toObject()["content"].toString().contains("dalmışım"));
        QVERIFY(!messages[1].toObject()["content"].toString().contains("changed elsewhere"));
        QCOMPARE(messages[2].toObject()["role"].toString(), QString("assistant"));
        QCOMPARE(messages[3].toObject()["content"].toString(), QString("Why use -mış here?"));
    }
    void retryReplacesOnlyTheInterruptedTurnAndExcludesPartialAnswers() {
        Conversation chat;
        chat.begin({{"selection", "ışık"}});
        chat.append("Light");
        chat.finish("complete");
        chat.followUp("What is the suffix?");
        chat.append("An incomplete answer");
        chat.finish("incomplete", "Stopped");
        chat.retry();
        QCOMPARE(chat.turns().size(), 2);
        QVERIFY(chat.latest().answer.isEmpty());
        QVERIFY(!QJsonDocument(chat.messages()).toJson().contains("An incomplete answer"));
        chat.append("Answer");
        chat.finish("failed");
        chat.followUp("Another question");
        QCOMPARE(chat.messages().size(), 4);
        QVERIFY(!QJsonDocument(chat.messages()).toJson().contains("What is the suffix?"));
    }
    void limitsRejectWithoutDiscardingHistory() {
        Conversation chat;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, chat.begin({{"selection", QString(4001, 'x')}}));
        chat.begin({{"selection", "Türkçe"}});
        chat.append("Meaning");
        chat.finish("complete");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, chat.followUp(QString(2001, 'x')));
        QCOMPARE(chat.turns().size(), 1);
        chat.followUp("More?");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, chat.append(QString(65537, 'x')));
        QVERIFY(chat.latest().answer.isEmpty());
        chat.finish("failed");
        chat.clear();
        QVERIFY(chat.empty());
        QVERIFY(!chat.busy());
    }
};
QTEST_GUILESS_MAIN(ConversationTests)
#include "conversation_tests.moc"
