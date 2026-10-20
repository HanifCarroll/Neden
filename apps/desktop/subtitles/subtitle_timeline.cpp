#include "subtitle_timeline.h"
#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>
#include <algorithm>
#include <stdexcept>
const QVector<Cue> &SubtitleTimeline::entries(SubtitleRole role) const {
    return role == SubtitleRole::Primary ? transcriptModel.entries() : secondaryCues;
}
int SubtitleTimeline::count(SubtitleRole role) const {
    return static_cast<int>(entries(role).size());
}
const Cue *SubtitleTimeline::cue(int index, SubtitleRole role) const {
    const auto &track = entries(role);
    return index >= 0 && index < track.size() ? &track[index] : nullptr;
}
int SubtitleTimeline::active(qint64 positionMs, SubtitleRole role) const {
    return cueAt(entries(role), positionMs);
}
QString SubtitleTimeline::text(qint64 positionMs, SubtitleRole role) const {
    const auto *item = cue(active(positionMs, role), role);
    if (item)
        return item->text;
    const auto *primary = cue(active(positionMs));
    return role == SubtitleRole::Secondary && secondaryCues.isEmpty() && primary ? primary->translation
                                                                                 : QString();
}
QString SubtitleTimeline::path(SubtitleRole role) const {
    return role == SubtitleRole::Primary ? primarySubtitlePath : secondarySubtitlePath;
}
QString SubtitleTimeline::name(SubtitleRole role) const {
    return QFileInfo(path(role)).fileName();
}
QVariantList SubtitleTimeline::tracks() const {
    QVariantList result;
    for (const auto &path : loadedSubtitlePaths)
        result.append(QVariantMap{{"path", path},
                                  {"name", QFileInfo(path).fileName()},
                                  {"primary", path == primarySubtitlePath},
                                  {"secondary", path == secondarySubtitlePath}});
    return result;
}
QVector<Cue> SubtitleTimeline::read(const QUrl &url) {
    if (!url.isLocalFile())
        throw std::runtime_error("Choose a local subtitle file.");
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
        throw std::runtime_error("Cannot read subtitle file (8 MiB maximum).");
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(file.readAll());
    if (decoder.hasError())
        throw std::runtime_error("Subtitles must use UTF-8 encoding.");
    return parseSrt(text);
}
void SubtitleTimeline::publish(QVector<Cue> cues, const QString &path, SubtitleRole role) {
    if (role == SubtitleRole::Primary) {
        primarySubtitlePath = path;
        transcriptModel.replace(std::move(cues));
    } else {
        secondarySubtitlePath = path;
        secondaryCues = std::move(cues);
    }
    if (!path.isEmpty() && !loadedSubtitlePaths.contains(path))
        loadedSubtitlePaths.append(path);
    transcriptModel.setTranslationCues(secondaryCues);
}
void SubtitleTimeline::load(const QUrl &url, SubtitleRole role) {
    publish(read(url), QFileInfo(url.toLocalFile()).absoluteFilePath(), role);
}
void SubtitleTimeline::replacePrimary(QVector<Cue> cues) {
    transcriptModel.replace(std::move(cues));
    transcriptModel.setTranslationCues(secondaryCues);
}
void SubtitleTimeline::clear(SubtitleRole role) {
    publish({}, {}, role);
}
void SubtitleTimeline::reset() {
    secondaryCues.clear();
    transcriptModel.replace({});
    primarySubtitlePath.clear();
    secondarySubtitlePath.clear();
    loadedSubtitlePaths.clear();
}
int SubtitleTimeline::previous(qint64 positionMs, int pending) const {
    if (pending >= 0)
        return pending - 1;
    const int current = active(positionMs);
    if (current >= 0)
        return current - 1;
    const auto &track = entries(SubtitleRole::Primary);
    const auto next = std::lower_bound(track.begin(), track.end(), positionMs,
                                       [](const Cue &cue, qint64 time) { return cue.startMs < time; });
    return next == track.begin() ? -1 : static_cast<int>(next - track.begin()) - 1;
}
int SubtitleTimeline::next(qint64 positionMs, int pending) const {
    if (pending >= 0)
        return pending + 1 < count() ? pending + 1 : -1;
    const auto &track = entries(SubtitleRole::Primary);
    const auto next = std::upper_bound(track.begin(), track.end(), positionMs,
                                       [](qint64 time, const Cue &cue) { return time < cue.startMs; });
    return next == track.end() ? -1 : static_cast<int>(next - track.begin());
}
StudyTarget SubtitleTimeline::capture(StudyTarget target, bool includeSecondary) const {
    target.before.clear();
    target.after.clear();
    target.secondary.clear();
    target.translation.clear();
    const auto role = target.role;
    const int index = active(target.positionMs, role);
    const auto *item = cue(index, role);
    if (!item)
        throw std::runtime_error("There is no subtitle at this position.");
    if (target.selection.isEmpty())
        target.selection = item->text;
    target.sourceCue = item->text;
    target.cueIndex = index;
    target.startMs = item->startMs;
    target.endMs = item->endMs;
    if (role == SubtitleRole::Secondary)
        target.sourceLanguage = "auto";
    const auto &track = entries(role);
    for (int neighborIndex = qMax(0, index - 3); neighborIndex < index; ++neighborIndex)
        target.before.append(track[neighborIndex].text);
    for (int neighborIndex = index + 1; neighborIndex < qMin(static_cast<int>(track.size()), index + 4);
         ++neighborIndex)
        target.after.append(track[neighborIndex].text);
    const auto otherTrackRole =
        role == SubtitleRole::Primary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    for (const auto &clue : entries(otherTrackRole)) {
        if (clue.startMs < item->endMs && clue.endMs > item->startMs) {
            if (target.translation.isEmpty())
                target.translation = clue.text;
            if (includeSecondary)
                target.secondary.append(clue.text);
        }
    }
    if (target.translation.isEmpty())
        target.translation = item->translation;
    return target;
}
