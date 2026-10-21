#include "recent_videos.h"
#include "media_path.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <algorithm>

namespace {
constexpr qint64 maximumHistoryBytes = 4 * 1024 * 1024;
constexpr int maximumRecentVideos = 200;

bool validPath(const QString &path) {
    return !path.isEmpty() && path.size() <= 16384 && !path.contains(QChar::Null) &&
           QFileInfo(path).isAbsolute();
}

bool validTrackId(const QString &id) {
    if (id.isEmpty())
        return true;
    bool valid = false;
    const auto number = id.toLongLong(&valid);
    return valid && number >= 0 && QString::number(number) == id;
}
bool validVideo(const RecentVideo &video) {
    return std::isfinite(video.primaryDelay) && std::abs(video.primaryDelay) <= 600 &&
           std::isfinite(video.secondaryDelay) && std::abs(video.secondaryDelay) <= 600 &&
           validTrackId(video.primaryNativeTrack) && validTrackId(video.secondaryNativeTrack) &&
           validPath(video.path) && std::isfinite(video.position) && video.position >= 0 &&
           std::isfinite(video.duration) && video.duration >= 0 &&
           (video.primarySubtitlePath.isEmpty() || validPath(video.primarySubtitlePath)) &&
           (video.secondarySubtitlePath.isEmpty() || validPath(video.secondarySubtitlePath));
}
} // namespace

RecentVideos::RecentVideos(QString directory, QObject *parent)
    : QAbstractListModel(parent), storageFile(QDir(directory).filePath("recent-videos.json")) {
    load();
}

int RecentVideos::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : videos.size();
}

QVariant RecentVideos::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= videos.size())
        return {};

    const auto &video = videos[index.row()];
    switch (role) {
    case MediaTitle:
        return QFileInfo(video.path).completeBaseName();
    case MediaUrl:
        return QUrl::fromLocalFile(video.path);
    case FilePath:
        return video.path;
    case ResumePosition:
        return video.position;
    case DurationSeconds:
        return video.duration;
    case FileAvailable:
        return QFileInfo(video.path).isFile() && QFileInfo(video.path).isReadable();
    case Finished:
        return video.finished;
    default:
        return {};
    }
}

QHash<int, QByteArray> RecentVideos::roleNames() const {
    return {{MediaTitle, "mediaTitle"},
            {MediaUrl, "mediaUrl"},
            {FilePath, "filePath"},
            {ResumePosition, "resumePosition"},
            {DurationSeconds, "durationSeconds"},
            {FileAvailable, "fileAvailable"},
            {Finished, "finished"}};
}

std::optional<RecentVideo> RecentVideos::find(const QString &path) const {
    const auto identity = mediaPathIdentity(path);
    for (const auto &video : videos) {
        if (video.path == identity)
            return video;
    }
    return std::nullopt;
}

void RecentVideos::setError(const QString &message) {
    if (errorText == message)
        return;

    errorText = message;
    emit errorChanged();
}

void RecentVideos::load() {

    // 1. Bound and validate the entire document before accepting any saved entries.
    QFile file(storageFile);
    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumHistoryBytes) {
        validStorage = false;
    } else {
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        const auto root = document.object();
        const auto entries = root.value("videos").toArray();
        const int version = root.value("version").toInt();
        validStorage = error.error == QJsonParseError::NoError && document.isObject() &&
                       (version >= 1 && version <= 4) && root.value("videos").isArray() &&
                       entries.size() <= maximumRecentVideos;
        QSet<QString> paths;
        for (const auto &entry : entries) {
            const auto item = entry.toObject();
            RecentVideo video{item.value("path").toString(), item.value("position").toDouble(-1),
                              item.value("duration").toDouble(-1),
                              item.value("primarySubtitlePath").toString(),
                              item.value("secondarySubtitlePath").toString()};
            video.primaryDelay = item.value("primaryDelay").toDouble();
            video.secondaryDelay = item.value("secondaryDelay").toDouble();
            video.primaryNativeTrack = item.value("primaryNativeTrack").toString();
            video.secondaryNativeTrack = item.value("secondaryNativeTrack").toString();
            video.finished = item.value("finished").toBool();
            const bool validTrackFields =
                (!item.contains("primarySubtitlePath") || item.value("primarySubtitlePath").isString()) &&
                (!item.contains("secondarySubtitlePath") || item.value("secondarySubtitlePath").isString());
            if (!entry.isObject() || !validTrackFields || !validVideo(video) ||
                (item.contains("finished") && !item.value("finished").isBool())) {
                validStorage = false;
                break;
            }
            video.path = mediaPathIdentity(video.path);
            if (paths.contains(video.path))
                continue;

            paths.insert(video.path);
            videos.append(video);
        }
    }

    // 2. Preserve malformed files rather than replacing the user's saved history.
    if (!validStorage) {
        videos.clear();
        setError(
            "Recent videos could not be read. The saved file is preserved; history updates are disabled.");
    }
}

bool RecentVideos::persist(const QList<RecentVideo> &next) {
    if (!validStorage)
        return false;

    // 1. Save local file references and playback positions, never subtitle contents or credentials.
    QJsonArray entries;
    for (const auto &video : next) {
        QJsonObject item{{"path", video.path}, {"position", video.position}, {"duration", video.duration}};
        if (!video.primarySubtitlePath.isEmpty())
            item.insert("primarySubtitlePath", video.primarySubtitlePath);
        if (!video.secondarySubtitlePath.isEmpty())
            item.insert("secondarySubtitlePath", video.secondarySubtitlePath);
        item.insert("primaryDelay", video.primaryDelay);
        item.insert("secondaryDelay", video.secondaryDelay);
        item.insert("primaryNativeTrack", video.primaryNativeTrack);
        item.insert("secondaryNativeTrack", video.secondaryNativeTrack);
        item.insert("finished", video.finished);
        entries.append(item);
    }
    const auto bytes = QJsonDocument(QJsonObject{{"version", 4}, {"videos", entries}}).toJson();

    // 2. Commit atomically so an interrupted update cannot truncate existing history.
    QSaveFile file(storageFile);
    if (bytes.size() > maximumHistoryBytes || !QDir().mkpath(QFileInfo(storageFile).absolutePath()) ||
        !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError("Recent videos could not be saved. Check free space and folder permissions.");
        return false;
    }

    setError({});
    return true;
}

bool RecentVideos::remember(const RecentVideo &requested, bool promote) {
    if (!validVideo(requested))
        return false;

    auto video = requested;
    video.path = mediaPathIdentity(video.path);

    // 1. Update an existing row in place unless an explicit open promotes it.
    auto next = videos;
    int existing = -1;
    for (int row = 0; row < next.size(); ++row) {
        if (next[row].path == video.path) {
            existing = row;
            break;
        }
    }
    if (existing >= 0 && !promote) {
        // Periodic checkpoints must retain an explicit completion decision.
        video.finished = next[existing].finished;
        next[existing] = video;
    } else {
        if (existing >= 0)
            next.removeAt(existing);
        next.prepend(video);
        if (next.size() > maximumRecentVideos)
            next.removeLast();
    }
    if (!persist(next))
        return false;

    // 2. Keep list focus and scroll position stable during periodic position saves.
    if (existing >= 0 && !promote) {
        videos = next;
        emit dataChanged(index(existing), index(existing));
    } else {
        beginResetModel();
        videos = next;
        endResetModel();
        emit countChanged();
    }
    return true;
}

void RecentVideos::setFinished(const QUrl &url, bool finished) {
    if (!url.isLocalFile())
        return;
    const auto identity = mediaPathIdentity(url.toLocalFile());
    auto next = videos;
    for (int row = 0; row < next.size(); ++row) {
        if (next[row].path != identity || next[row].finished == finished)
            continue;
        next[row].finished = finished;
        if (!persist(next))
            return;
        videos = next;
        emit dataChanged(index(row), index(row), {Finished});
        return;
    }
}

void RecentVideos::remove(const QUrl &url) {
    if (!url.isLocalFile())
        return;

    const auto identity = mediaPathIdentity(url.toLocalFile());
    auto next = videos;
    for (int row = 0; row < next.size(); ++row) {
        if (next[row].path != identity)
            continue;

        next.removeAt(row);
        if (!persist(next))
            return;

        beginRemoveRows({}, row, row);
        videos = next;
        endRemoveRows();
        emit countChanged();
        emit removed(identity);
        return;
    }
}

void RecentVideos::clear() {
    if (!persist({}))
        return;
    const auto previous = videos;
    beginResetModel();
    videos.clear();
    endResetModel();
    emit countChanged();
    for (const auto &video : previous)
        emit removed(video.path);
}
bool RecentVideos::relocate(const QUrl &oldUrl, const QUrl &newUrl) {
    if (!oldUrl.isLocalFile() || !newUrl.isLocalFile() || !QFileInfo(newUrl.toLocalFile()).isFile())
        return false;
    auto saved = find(oldUrl.toLocalFile());
    if (!saved)
        return false;
    const auto oldPath = saved->path;
    saved->path = mediaPathIdentity(newUrl.toLocalFile());
    auto next = videos;
    next.erase(std::remove_if(next.begin(), next.end(),
                              [&](const RecentVideo &video) {
                                  return video.path == oldPath || video.path == saved->path;
                              }),
               next.end());
    next.prepend(*saved);
    if (!persist(next))
        return false;
    beginResetModel();
    videos = next;
    endResetModel();
    emit countChanged();
    emit removed(oldPath);
    return true;
}

void RecentVideosFilter::setHistory(RecentVideos *history) {
    if (sourceModel() == history)
        return;
    setSourceModel(history);
    emit historyChanged();
}

void RecentVideosFilter::setIncludeFinished(bool include) {
    if (showFinished == include)
        return;
    beginFilterChange();
    showFinished = include;
    endFilterChange(Direction::Rows);
    emit includeFinishedChanged();
}

bool RecentVideosFilter::filterAcceptsRow(int row, const QModelIndex &parent) const {
    const auto entry = sourceModel()->index(row, 0, parent);
    return showFinished || !sourceModel()->data(entry, RecentVideos::Finished).toBool();
}
