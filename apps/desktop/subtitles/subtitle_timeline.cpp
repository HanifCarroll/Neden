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
QVector<Cue> SubtitleTimeline::read(const QUrl &url, const QString &encoding) {
    if (!url.isLocalFile())
        throw std::runtime_error("Choose a local subtitle file.");
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
        throw std::runtime_error("Cannot read subtitle file (8 MiB maximum).");
    const auto bytes = file.readAll();
    QStringDecoder decoder(
        encoding == "auto"
            ? QStringConverter::encodingForData(bytes).value_or(QStringConverter::Utf8)
            : QStringConverter::encodingForName(encoding.toUtf8()).value_or(QStringConverter::Utf8));
    QString text;
    if (encoding == "windows-1254" || encoding == "windows-1252" || encoding == "ISO-8859-9") {
        // Unicode's single-byte mapping; never guess a legacy code page from valid UTF-8.
        constexpr char16_t punctuation[] = {0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
                                            0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
                                            0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
                                            0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178};
        for (const unsigned char byte : bytes) {
            char16_t character = byte;
            if (encoding != "ISO-8859-9" && byte >= 0x80 && byte < 0xa0)
                character = punctuation[byte - 0x80];
            if (encoding != "windows-1252") {
                switch (byte) {
                case 0xd0:
                    character = u'Ğ';
                    break;
                case 0xdd:
                    character = u'İ';
                    break;
                case 0xde:
                    character = u'Ş';
                    break;
                case 0xf0:
                    character = u'ğ';
                    break;
                case 0xfd:
                    character = u'ı';
                    break;
                case 0xfe:
                    character = u'ş';
                    break;
                }
            }
            text.append(QChar(character));
        }
    } else {
        text = decoder.decode(bytes);
        if (decoder.hasError())
            throw std::runtime_error("This subtitle file is not valid Unicode. Choose its encoding in "
                                     "Playback options to match the file, then load it again.");
    }
    return parseSrt(text);
}
void SubtitleTimeline::publish(QVector<Cue> cues, const QString &path, SubtitleRole role) {
    if (role == SubtitleRole::Primary) {
        primarySubtitlePath = path;
        originalPrimary = std::move(cues);
    } else {
        secondarySubtitlePath = path;
        originalSecondary = std::move(cues);
    }
    rebuild();
    if (!path.isEmpty() && !loadedSubtitlePaths.contains(path))
        loadedSubtitlePaths.append(path);
    transcriptModel.setTranslationCues(secondaryCues);
}
void SubtitleTimeline::load(const QUrl &url, SubtitleRole role, const QString &encoding) {
    publish(read(url, encoding), QFileInfo(url.toLocalFile()).absoluteFilePath(), role);
}
void SubtitleTimeline::replacePrimary(QVector<Cue> cues) {
    originalPrimary = std::move(cues);
    rebuild();
}
void SubtitleTimeline::clear(SubtitleRole role) {
    publish({}, {}, role);
}
void SubtitleTimeline::reset() {
    originalPrimary.clear();
    originalSecondary.clear();
    primaryDelay = secondaryDelay = 0;
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

void SubtitleTimeline::setDelay(qint64 milliseconds, SubtitleRole role) {
    if (role == SubtitleRole::Primary)
        primaryDelay = milliseconds;
    else
        secondaryDelay = milliseconds;
    rebuild();
}
void SubtitleTimeline::rebuild() {
    auto primary = originalPrimary;
    secondaryCues = originalSecondary;
    for (auto &cue : primary) {
        cue.startMs += primaryDelay;
        cue.endMs += primaryDelay;
    }
    for (auto &cue : secondaryCues) {
        cue.startMs += secondaryDelay;
        cue.endMs += secondaryDelay;
    }
    transcriptModel.replace(std::move(primary));
    transcriptModel.setTranslationCues(secondaryCues);
}
