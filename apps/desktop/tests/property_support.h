#pragma once
#include <QtTest>
#include <random>

namespace PropertySupport {
// Each row is independently replayable: NEDEN_PROPERTY_SEED=<row seed> NEDEN_PROPERTY_RUNS=1.
inline void seeds(int defaultRuns = 64) {
    bool ok = true;
    const auto seedText = qgetenv("NEDEN_PROPERTY_SEED");
    const uint first = seedText.isEmpty() ? 0x4e454445U : seedText.toUInt(&ok);
    if (!ok)
        qFatal("NEDEN_PROPERTY_SEED must be an unsigned 32-bit integer");
    const auto runsText = qgetenv("NEDEN_PROPERTY_RUNS");
    const int runs = runsText.isEmpty() ? defaultRuns : runsText.toInt(&ok);
    if (!ok || runs < 1 || runs > 10000)
        qFatal("NEDEN_PROPERTY_RUNS must be between 1 and 10000");
    QTest::addColumn<uint>("seed");
    for (int index = 0; index < runs; ++index) {
        const uint seed = first + uint(index);
        QTest::newRow(qPrintable(QString("seed-%1").arg(seed))) << seed;
    }
}

inline int number(std::mt19937 &random, int low, int high) {
    return low + int(random() % uint(high - low + 1));
}

inline QString text(std::mt19937 &random) {
    const QStringList words{"İstanbul", "ışık",           "日本語", "مرحبا",     "cafe\u0301", "🙂",
                            "A&B",      "<i>literal</i>", "&amp;",  "<unknown>", "\"quoted\"", "line\nnext"};
    QString result = "Text";
    for (int index = number(random, 1, 8); index > 0; --index)
        result += ' ' + words[number(random, 0, words.size() - 1)];
    return result;
}
} // namespace PropertySupport
