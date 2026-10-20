#include "subtitle_timeline.h"
#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>
#include <algorithm>
#include <stdexcept>
const QVector<Cue> &SubtitleTimeline::entries(SubtitleRole role) const {
    return role == SubtitleRole::Primary ? model.entries() : secondary;
}
int SubtitleTimeline::count(SubtitleRole role) const {
    return int(entries(role).size());
}
const Cue *SubtitleTimeline::cue(int index, SubtitleRole role) const {
    const auto &track = entries(role);
    return index >= 0 && index < track.size() ? &track[index] : nullptr;
}
int SubtitleTimeline::active(qint64 ms, SubtitleRole role) const {
    return cueAt(entries(role), ms);
}
QString SubtitleTimeline::text(qint64 ms, SubtitleRole role) const {
    const auto *item = cue(active(ms, role), role);
    if (item)
        return item->text;
    const auto *primary = cue(active(ms));
    return role == SubtitleRole::Secondary && secondary.isEmpty() && primary ? primary->translation
                                                                             : QString();
}
QString SubtitleTimeline::path(SubtitleRole role) const {
    return role == SubtitleRole::Primary ? primaryPath : secondaryPath;
}
QString SubtitleTimeline::name(SubtitleRole role) const {
    return QFileInfo(path(role)).fileName();
}
QVariantList SubtitleTimeline::tracks() const {
    QVariantList result;
    for (const auto &path : loadedPaths)
        result.append(QVariantMap{{"path", path},
                                  {"name", QFileInfo(path).fileName()},
                                  {"primary", path == primaryPath},
                                  {"secondary", path == secondaryPath}});
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
        primaryPath = path;
        model.replace(std::move(cues));
    } else {
        secondaryPath = path;
        secondary = std::move(cues);
    }
    if (!path.isEmpty() && !loadedPaths.contains(path))
        loadedPaths.append(path);
    model.setTranslationCues(secondary);
}
void SubtitleTimeline::load(const QUrl &url, SubtitleRole role) {
    publish(read(url), QFileInfo(url.toLocalFile()).absoluteFilePath(), role);
}
void SubtitleTimeline::replacePrimary(QVector<Cue> cues) {
    model.replace(std::move(cues));
    model.setTranslationCues(secondary);
}
void SubtitleTimeline::clear(SubtitleRole role) {
    publish({}, {}, role);
}
void SubtitleTimeline::reset() {
    secondary.clear();
    model.replace({});
    primaryPath.clear();
    secondaryPath.clear();
    loadedPaths.clear();
}
int SubtitleTimeline::previous(qint64 ms, int pending) const {
    if (pending >= 0)
        return pending - 1;
    const int current = active(ms);
    if (current >= 0)
        return current - 1;
    const auto &track = entries(SubtitleRole::Primary);
    const auto next = std::lower_bound(track.begin(), track.end(), ms,
                                       [](const Cue &cue, qint64 time) { return cue.startMs < time; });
    return next == track.begin() ? -1 : int(next - track.begin()) - 1;
}
int SubtitleTimeline::next(qint64 ms, int pending) const {
    if (pending >= 0)
        return pending + 1 < count() ? pending + 1 : -1;
    const auto &track = entries(SubtitleRole::Primary);
    const auto next = std::upper_bound(track.begin(), track.end(), ms,
                                       [](qint64 time, const Cue &cue) { return time < cue.startMs; });
    return next == track.end() ? -1 : int(next - track.begin());
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
    for (int i = qMax(0, index - 3); i < index; ++i)
        target.before.append(track[i].text);
    for (int i = index + 1; i < qMin(int(track.size()), index + 4); ++i)
        target.after.append(track[i].text);
    const auto other = role == SubtitleRole::Primary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    for (const auto &clue : entries(other)) {
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
