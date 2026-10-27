#pragma once
#include "ai_client.h"
#include "app_settings.h"
#include "cues.h"
#include <QPointer>
#include <QNetworkReply>

// Translates a frozen document in bounded, contextual batches. Publication happens
// only after every cue has been validated; cancellation discards all partial work.
class SubtitleTranslation : public QObject {
    Q_OBJECT
public:
    SubtitleTranslation(AppSettings &settings, AiClient::CredentialReader credentials,
                        QObject *parent = nullptr);
    void setHostedAccount(HostedAccount *account) { client.setHostedAccount(account); }
    void start(QVector<Cue> source, const QString &targetLanguage);
    void cancel();
    static QStringList parseBatch(const QByteArray &body, int firstId, int count);
    static QString prompt();
    static int batchEnd(const QVector<Cue> &source, int start);
signals:
    void progress(int completed, int total);
    void ready(QVector<Cue> cues);
    void failed(QString message);

private:
    void nextBatch();
    AppSettings &settings;
    AiClient client;
    QPointer<QNetworkReply> reply;
    QVector<Cue> source, translated;
    QString language;
    int completed = 0;
};
