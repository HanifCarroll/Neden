#pragma once
#include "subtitle_translation.h"
#include <QThread>
#include <atomic>
#include <functional>
#include <memory>

class SubtitleTools : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Subtitle tools belong to the session")
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool hasRepair READ hasRepair NOTIFY changed)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY changed)
    Q_PROPERTY(int translationBatches READ translationBatches NOTIFY changed)
public:
    using Decoder = std::function<QVector<Cue>(const QString &, int, const std::atomic_bool &)>;
    SubtitleTools(QString directory, AppSettings &settings, AiClient::CredentialReader credentials,
                  QObject *parent = nullptr);
    ~SubtitleTools() override;
    void setDecoder(Decoder value) { decoder = std::move(value); }
    void setHostedAccount(HostedAccount *account) { translation.setHostedAccount(account); }
    void prepare(QVector<Cue> source, QVector<Cue> reference, bool secondary);
    void extract(const QString &path, int stream, bool secondary);
    bool busy() const;
    QString message() const { return status; }
    bool hasRepair() const { return !proposed.isEmpty(); }
    int lineCount() const { return source.size(); }
    int translationBatches() const;
    Q_INVOKABLE QString lineText(int index) const;
    Q_INVOKABLE void translate(const QString &language);
    Q_INVOKABLE void alignToReference();
    Q_INVOKABLE void matchLines(int firstLine, double firstSeconds, int secondLine = -1,
                                double secondSeconds = -1);
    Q_INVOKABLE void applyRepair();
    Q_INVOKABLE void cancel();
    void invalidate();
signals:
    void changed();
    void trackReady(QString path, bool secondary, bool preserveDelay);

private:
    struct Job {
        QThread *thread;
        std::shared_ptr<std::atomic_bool> cancelled;
    };
    struct Extraction {
        QString path;
        int stream;
        bool secondary;
    };
    void startQueuedExtractions();
    void launch(std::function<QVector<Cue>(const std::atomic_bool &)> work,
                std::function<void(QVector<Cue>)> accept);
    void publish(const QVector<Cue> &cues, const QString &label, bool secondary, bool preserveDelay = false);
    QString directory, status, targetLanguage;
    bool sourceSecondary = false, translating = false;
    quint64 generation = 0;
    QVector<Cue> source, reference, proposed;
    QVector<Job> jobs;
    QList<Extraction> queuedExtractions;
    Decoder decoder;
    SubtitleTranslation translation;
};
