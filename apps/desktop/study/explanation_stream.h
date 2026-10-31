#pragma once
#include <QByteArray>
#include <QStringList>

// Pure decoder shared by the network session and byte-boundary property tests.
class ExplanationStream {
public:
    QStringList append(const QByteArray &bytes);
    bool complete() const {
        return sawDone && !truncated && failure.isEmpty() && pending.trimmed().isEmpty();
    }
    QString error() const { return failure; }

private:
    QByteArray pending;
    qsizetype answerCharacters = 0;
    bool sawDone = false, truncated = false;
    QString failure;
};
