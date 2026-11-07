#pragma once
#include "study_target.h"
#include <QObject>
#include <QThread>
#include <atomic>

class AppSettings;

// Local-only, best-effort append queue. Disk I/O never runs on the playback/UI thread.
class LookupHistory {
public:
    LookupHistory(QString directory, const AppSettings &settings);
    ~LookupHistory();
    QJsonObject begin(const QString &kind, const StudyTarget &target, int offset = -1);
    void complete(QJsonObject event, QJsonObject result);
    // Drain queued writes on shutdown or in tests, never on a playback gesture.
    void flush();

private:
    void append(const QJsonObject &event);
    const AppSettings &settings;
    QString directory;
    QThread writerThread;
    QObject *writer = nullptr;
    std::atomic_int queued{0};
};
