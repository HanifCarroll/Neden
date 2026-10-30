#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QStringList>
#include <QVariantMap>

// Decodes bounded SSE and exposes only complete, validated word entries.
class WordBatch {
public:
    static QString prompt();
    void reset(const QStringList &words);
    void append(const QByteArray &bytes);
    QMap<QString, QVariantMap> entries() const;
    bool complete() const;
private:
    QStringList expectedWords;
    QByteArray pending;
    QString content;
    qsizetype receivedBytes = 0;
    bool ended = false;
    bool stopped = false;
};
