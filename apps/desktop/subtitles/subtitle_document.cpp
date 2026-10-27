#include "subtitle_document.h"
#include <QDir>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr qint64 maximumTime = 24 * 60 * 60 * 1000;
QString timestamp(qint64 milliseconds) {
    return QString("%1:%2:%3,%4")
        .arg(milliseconds / 3600000, 2, 10, QChar('0'))
        .arg(milliseconds / 60000 % 60, 2, 10, QChar('0'))
        .arg(milliseconds / 1000 % 60, 2, 10, QChar('0'))
        .arg(milliseconds % 1000, 3, 10, QChar('0'));
}
void validate(const QVector<Cue> &cues) {
    if (cues.isEmpty() || cues.size() > 50000)
        throw std::runtime_error("Choose a text subtitle track with 1–50,000 lines.");
    for (const auto &cue : cues)
        if (cue.startMs < 0 || cue.endMs <= cue.startMs || cue.endMs > maximumTime ||
            cue.text.trimmed().isEmpty() || cue.text.size() > 4000)
            throw std::runtime_error("This track contains invalid subtitle text or timing.");
}
} // namespace
QByteArray SubtitleDocument::serialize(const QVector<Cue> &cues) {
    validate(cues);
    QString output;
    int index = 0;
    for (const auto &cue : cues) {
        QStringList lines;
        for (const auto &line : cue.text.split('\n'))
            if (!line.trimmed().isEmpty())
                lines.append(line.trimmed());
        output += QString::number(++index) + '\n' + timestamp(cue.startMs) + " --> " + timestamp(cue.endMs) +
                  '\n' + lines.join('\n') + "\n\n";
    }
    const auto bytes = output.toUtf8();
    if (bytes.size() > 8 * 1024 * 1024)
        throw std::runtime_error("Generated subtitles exceed 8 MiB.");
    return bytes;
}
QString SubtitleDocument::save(const QVector<Cue> &cues, const QString &directory, const QString &label) {
    const auto bytes = serialize(cues);
    const QString folder = directory + "/subtitles/generated";
    if (!QDir().mkpath(folder))
        throw std::runtime_error("Cannot create the subtitle folder.");
    // Labels are internal constants or catalog language names, never file paths.
    QString safeLabel;
    for (const auto character : label)
        if (character.isLetterOrNumber() || character == '-')
            safeLabel += character;
    const QString path =
        folder + '/' + safeLabel.left(60) + '-' + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".srt";
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("Cannot save the generated subtitle track.");
    return path;
}
SubtitleDocument::Timing SubtitleDocument::fromAnchors(qint64 firstSubtitle, qint64 firstVideo,
                                                       qint64 secondSubtitle, qint64 secondVideo) {
    if (firstSubtitle < 0 || firstVideo < 0 || firstVideo > maximumTime)
        throw std::runtime_error("Choose a subtitle line and its matching video position.");
    double rate = 1;
    if (secondSubtitle >= 0 || secondVideo >= 0) {
        if (secondSubtitle - firstSubtitle < 30000 || secondVideo <= firstVideo || secondVideo > maximumTime)
            throw std::runtime_error("Choose a second matching line at least 30 seconds later.");
        rate = double(secondVideo - firstVideo) / double(secondSubtitle - firstSubtitle);
        if (rate < 0.8 || rate > 1.25)
            throw std::runtime_error("These matches imply too much drift. Check the two matching lines.");
    }
    return {rate, double(firstVideo) - rate * double(firstSubtitle), 1};
}
QVector<Cue> SubtitleDocument::retime(const QVector<Cue> &source, Timing timing) {
    validate(source);
    if (!std::isfinite(timing.rate) || !std::isfinite(timing.offsetMs) || timing.rate < 0.8 ||
        timing.rate > 1.25 || std::abs(timing.offsetMs) > maximumTime)
        throw std::runtime_error("Invalid timing correction.");
    auto result = source;
    for (auto &cue : result) {
        cue.startMs = qRound64(cue.startMs * timing.rate + timing.offsetMs);
        cue.endMs = qRound64(cue.endMs * timing.rate + timing.offsetMs);
        if (cue.endMs <= 0)
            throw std::runtime_error(
                "This correction would remove opening dialogue. Check the timing matches.");
        cue.startMs = qMax(qint64(0), cue.startMs);
        cue.translation.clear();
    }
    validate(result);
    return result;
}
SubtitleDocument::Timing SubtitleDocument::align(const QVector<Cue> &source, const QVector<Cue> &reference,
                                                 const std::atomic_bool &cancelled) {
    validate(source);
    validate(reference);
    if (source.size() < 20 || reference.size() < 20)
        throw std::runtime_error(
            "Automatic alignment needs at least 20 lines in each track. Use matching lines instead.");
    // Compare speech occupancy, not translated words or cue numbers. Sampling across
    // the whole track prevents an opening scene alone from determining the repair.
    constexpr int resolution = 50;
    const qint64 end =
        std::max_element(reference.cbegin(), reference.cend(), [](const Cue &left, const Cue &right) {
            return left.endMs < right.endMs;
        })->endMs;
    QVector<int> occupancy(int(end / resolution) + 2, 0);
    for (const auto &cue : reference)
        for (int i = int(cue.startMs / resolution); i < int(cue.endMs / resolution); ++i)
            occupancy[i] = 1;
    QVector<int> prefix(occupancy.size() + 1, 0);
    for (int i = 0; i < occupancy.size(); ++i)
        prefix[i + 1] = prefix[i] + occupancy[i];
    const double density = double(prefix.last()) / occupancy.size();
    if (density > 0.8 || density < 0.02)
        throw std::runtime_error("The reference has too little timing detail for reliable alignment.");
    auto score = [&](double rate, double offset) {
        double matched = 0, total = 0;
        const int stride = std::max(1, int(source.size() / 1000));
        for (int i = 0; i < source.size(); i += stride) {
            const auto &cue = source[i];
            const int start = int(std::floor((cue.startMs * rate + offset) / resolution));
            const int finish = int(std::ceil((cue.endMs * rate + offset) / resolution));
            total += std::max(1, finish - start);
            const int clippedStart = qBound(0, start, int(occupancy.size()));
            const int clippedEnd = qBound(0, finish, int(occupancy.size()));
            matched += prefix[clippedEnd] - prefix[clippedStart];
        }
        return (matched / total - density) / (1.0 - density);
    };
    Timing best{1, 0, -1};
    const double rates[] = {1.0, 24.0 / 25, 25.0 / 24, 24000.0 / 25025, 25025.0 / 24000};
    for (double rate : rates) {
        for (int offset = -120000; offset <= 120000; offset += 250) {
            if (cancelled)
                throw std::runtime_error("Cancelled.");
            const double confidence = score(rate, offset);
            if (confidence > best.confidence)
                best = {rate, double(offset), confidence};
        }
    }
    const auto coarse = best;
    for (int drift = -10; drift <= 10; ++drift) {
        const double rate = coarse.rate + drift * 0.00005;
        for (int delta = -500; delta <= 500; delta += resolution) {
            if (cancelled)
                throw std::runtime_error("Cancelled.");
            const double confidence = score(rate, coarse.offsetMs + delta);
            if (confidence > best.confidence)
                best = {rate, coarse.offsetMs + delta, confidence};
        }
    }
    double alternate = -1;
    for (int offset = -120000; offset <= 120000; offset += 250) {
        if (cancelled)
            throw std::runtime_error("Cancelled.");
        if (std::abs(offset - best.offsetMs) > 2000)
            alternate = std::max(alternate, score(best.rate, offset));
    }
    if (best.confidence < 0.55 || best.confidence - alternate < 0.08)
        throw std::runtime_error(
            "No reliable timing match. Choose a track from the same cut, or match two lines manually.");
    return best;
}
