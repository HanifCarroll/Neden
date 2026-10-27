#pragma once
#include "cues.h"
#include <atomic>

namespace SubtitleDocument {
// Generated tracks are separate, atomic files. Original files are never modified.
QString save(const QVector<Cue> &cues, const QString &directory, const QString &label);
QByteArray serialize(const QVector<Cue> &cues);
struct Timing {
    double rate = 1.0;
    double offsetMs = 0;
    double confidence = 1.0;
};
Timing fromAnchors(qint64 firstSubtitle, qint64 firstVideo, qint64 secondSubtitle = -1,
                   qint64 secondVideo = -1);
Timing align(const QVector<Cue> &source, const QVector<Cue> &reference, const std::atomic_bool &cancelled);
QVector<Cue> retime(const QVector<Cue> &source, Timing timing);
} // namespace SubtitleDocument
