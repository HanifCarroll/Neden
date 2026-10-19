#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>
#include <QVector>

struct ConversationTurn {
    QString question;
    QString answer;
    QString status = "streaming";
    QString error;
    QString provider;
    bool fallback = false;
};

class Conversation {
public:
    void begin(const QJsonObject &context);
    void followUp(const QString &question);
    void retry();
    QJsonArray messages() const;
    void append(const QString &text);
    void finish(const QString &status, const QString &error = {});
    void setProvider(const QString &name, bool fallback);
    void clear();
    bool busy() const { return active; }
    bool empty() const { return history.isEmpty(); }
    const QJsonObject &context() const { return frozenContext; }
    const ConversationTurn &latest() const { return history.last(); }
    QVariantList turns() const;
private:
    void startLatest();
    QJsonObject frozenContext;
    QVector<ConversationTurn> history;
    bool active = false;
};
