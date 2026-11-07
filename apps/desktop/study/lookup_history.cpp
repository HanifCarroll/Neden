#include "lookup_history.h"
#include "app_settings.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QUuid>

LookupHistory::LookupHistory(QString directory, const AppSettings &settings)
    : settings(settings), directory(std::move(directory)) {}
LookupHistory::~LookupHistory() {
    flush();
    writerThread.quit();
    writerThread.wait();
}
QJsonObject LookupHistory::begin(const QString &kind, const StudyTarget &target, int offset) {
    if (!settings.learning().lookupHistoryEnabled)
        return {};
    QJsonObject event{{"v", 1},
                      {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                      {"at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                      {"kind", kind},
                      {"media", QFileInfo(target.mediaPath).absoluteFilePath()},
                      {"positionSec", target.positionMs / 1000.0},
                      {"cueText", target.historyCueText},
                      {"track", target.role == SubtitleRole::Primary ? "primary" : "secondary"},
                      {"selection", target.selection},
                      {"sourceLanguage", LanguageCatalog::code(target.sourceLanguage)},
                      {"explanationLanguage", LanguageCatalog::code(target.explanationLanguage)}};
    if (target.cueIndex >= 0) {
        event.insert("cueStartSec", target.startMs / 1000.0);
        event.insert("cueEndSec", target.endMs / 1000.0);
    }
    if (!target.historySecondaryText.isEmpty())
        event.insert("secondaryText", target.historySecondaryText);
    if (kind == "word" && offset >= 0)
        event.insert("offset", offset);
    append(event);
    return event;
}
void LookupHistory::complete(QJsonObject event, QJsonObject result) {
    if (event.isEmpty())
        return;
    if (event.value("kind") == "line") {
        const auto breakdown = result.value("breakdown").toString();
        qsizetype length = qMin(qsizetype(4000), breakdown.size());
        // Do not split a supplementary Unicode character at the truncation boundary.
        if (length > 0 && breakdown.at(length - 1).isHighSurrogate())
            --length;
        result.insert("breakdown", breakdown.left(length));
    }
    event.insert("result", result);
    append(event);
}
void LookupHistory::append(const QJsonObject &event) {
    if (!settings.learning().lookupHistoryEnabled || event.isEmpty() || queued.load() >= 256)
        return;
    if (!writer) {
        writer = new QObject;
        writer->moveToThread(&writerThread);
        QObject::connect(&writerThread, &QThread::finished, writer, &QObject::deleteLater);
        writerThread.start();
    }
    const auto bytes = QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
    ++queued;
    QMetaObject::invokeMethod(
        writer,
        [this, bytes] {
            // One worker and a non-waiting cross-process lock keep complete records together.
            if (QDir().mkpath(directory)) {
                QLockFile lock(directory + "/lookups.jsonl.lock");
                if (lock.tryLock(0)) {
                    QFile file(directory + "/lookups.jsonl");
                    if (file.open(QIODevice::ReadWrite | QIODevice::Append)) {
                        const auto previousSize = file.size();
                        if (file.write(bytes) != bytes.size() || !file.flush())
                            file.resize(previousSize);
                    }
                }
            }
            --queued;
        },
        Qt::QueuedConnection);
}
void LookupHistory::flush() {
    if (!writer)
        return;
    QMetaObject::invokeMethod(writer, [] {}, Qt::BlockingQueuedConnection);
}
