#pragma once
#include "conversation.h"
#include "study_target.h"
#include <optional>
#include "app_settings.h"
#include "ai_client.h"
#include <QPointer>
class ExplanationSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Explanations are supplied by the application")
    Q_PROPERTY(QString answer READ answer NOTIFY changed)
    Q_PROPERTY(QString subject READ subject NOTIFY changed)
    Q_PROPERTY(QString sourceCue READ sourceCue NOTIFY changed)
    Q_PROPERTY(QVariantList turns READ turns NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
public:
    ExplanationSession(AppSettings &settings, AiClient::CredentialReader credentialReader,
                       QObject *parent = nullptr, QString alternateApiEndpoint = "https://api.openai.com/v1");
    ~ExplanationSession() override { stop(); }
    QString answer() const { return answerText; }
    QString subject() const { return subjectText; }
    QString sourceCue() const { return subjectContext; }
    QVariantList turns() const { return conversation.turns(); }
    QString state() const { return requestState; }
    QString message() const { return statusText; }
    const std::optional<StudyTarget> &target() const { return activeTarget; }
    void explainTarget(const StudyTarget &target);
    const QJsonObject &context() const { return conversation.context(); }
    void explain(const QString &selection, const QString &context);
    void explainContext(const QJsonObject &context);
    Q_INVOKABLE bool followUp(const QString &question);
    Q_INVOKABLE void retryExplanation();
    Q_INVOKABLE void stop();
    void invalidateContext();
    void setHostedAccount(HostedAccount *account) { aiClient.setHostedAccount(account); }
signals:
    void changed();
    void replyStarted();

private:
    void begin(const StudyTarget &target, const QJsonObject &context);
    void launchConversation(bool fallback = false);
    void consumeEvents();
    void setMessage(const QString &message) {
        statusText = message;
        emit changed();
    }
    AppSettings &settings;
    AiClient aiClient;
    QString alternateEndpoint;
    QPointer<QNetworkReply> aiReply;
    QByteArray pendingEvents;
    QString answerText, subjectText, subjectContext, requestState = "idle",
                                                     statusText = "Select a subtitle to study.";
    Conversation conversation;
    std::optional<StudyTarget> activeTarget;
    bool fallbackWaiting = false, sawDone = false, truncated = false;
    quint64 requestGeneration = 0;
};
