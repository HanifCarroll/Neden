#pragma once
#include <QString>
#include <QVector>

struct WordSpan {
    int start = 0;
    int end = 0;
};

// Offsets are UTF-16 positions, matching QML TextEdit and QString.
QVector<WordSpan> dictionaryWordBoundaries(const QString &text);
