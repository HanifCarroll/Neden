#pragma once
#include "cues.h"
#include "study_target.h"
#include <QUrl>
#include <QVariantList>

// Owns both timelines, file decoding, transcript projection and context capture.
class SubtitleTimeline {
public:
    CueModel *transcript() { return &transcriptModel; }
    const Cue *cue(int index, SubtitleRole role = SubtitleRole::Primary) const;
    int count(SubtitleRole role = SubtitleRole::Primary) const;
    int active(qint64 milliseconds, SubtitleRole role = SubtitleRole::Primary) const;
    int previous(qint64 milliseconds, int pending = -1) const;
    int next(qint64 milliseconds, int pending = -1) const;
    QString text(qint64 milliseconds, SubtitleRole role) const;
    QString path(SubtitleRole role) const;
    QString name(SubtitleRole role) const;
    QVariantList tracks() const;
    QVector<Cue> snapshot(SubtitleRole role) const { return entries(role); }
    void load(const QUrl &url, SubtitleRole role, const QString &encoding = "auto");
    void setDelay(qint64 milliseconds, SubtitleRole role);
    void publish(QVector<Cue> cues, const QString &path, SubtitleRole role);
    void replacePrimary(QVector<Cue> cues);
    void clear(SubtitleRole role);
    void reset();
    bool knows(const QString &path) const { return loadedSubtitlePaths.contains(path); }
    StudyTarget capture(StudyTarget selection, bool includeSecondary) const;
    static QVector<Cue> read(const QUrl &url, const QString &encoding = "auto");

private:
    const QVector<Cue> &entries(SubtitleRole role) const;
    void rebuild();
    QVector<Cue> originalPrimary, originalSecondary;
    qint64 primaryDelay = 0, secondaryDelay = 0;
    CueModel transcriptModel;
    QVector<Cue> secondaryCues;
    QString primarySubtitlePath, secondarySubtitlePath;
    QStringList loadedSubtitlePaths;
};
