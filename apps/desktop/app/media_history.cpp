#include "media_history.h"
#include <cmath>

MediaHistory::MediaHistory(PlayerPreferences &preferences, QObject *parent)
    : QObject(parent), preferences(preferences) {
    checkpointTimer.setInterval(5000);
    connect(&checkpointTimer, &QTimer::timeout, this, &MediaHistory::checkpointRequested);
    checkpointTimer.start();
}

void MediaHistory::setHistory(RecentVideos *videos) {
    if (history)
        disconnect(history, nullptr, this, nullptr);
    history = videos;
    if (history)
        connect(history, &RecentVideos::removed, this, [this](const QString &path) {
            if (mediaPath == path)
                rememberCurrent = false;
        });
}

void MediaHistory::beginMedia(const QString &path, bool local) {
    mediaPath = path;
    ready = false;
    rememberCurrent = local;
    cancelResume();
}

std::optional<RecentVideo> MediaHistory::savedVideo() const {
    return history && preferences.rememberHistory() ? history->find(mediaPath) : std::nullopt;
}

double MediaHistory::prepareResume(double duration) {
    const auto saved = savedVideo();
    pendingResumeSeconds = saved && !saved->finished ? saved->position : -1;
    if (pendingResumeSeconds > 0 && duration > 0)
        pendingResumeSeconds = qMin(pendingResumeSeconds, duration);
    if (pendingResumeSeconds == 0)
        cancelResume();
    ready = true;
    return pendingResumeSeconds;
}

bool MediaHistory::canSave() const {
    return history && ready && rememberCurrent && preferences.rememberHistory();
}

void MediaHistory::recordOpened(RecentVideo snapshot) {
    if (!canSave())
        return;
    snapshot.path = mediaPath;
    if (pendingResumeSeconds >= 0)
        snapshot.position = pendingResumeSeconds;
    history->recordOpened(snapshot);
}

void MediaHistory::checkpoint(RecentVideo snapshot) {
    if (!canSave())
        return;
    snapshot.path = mediaPath;
    if (pendingResumeSeconds >= 0)
        snapshot.position = pendingResumeSeconds;
    history->saveProgress(snapshot);
}

void MediaHistory::observePosition(double seconds) {
    if (pendingResumeSeconds >= 0 && std::abs(seconds - pendingResumeSeconds) < 1)
        cancelResume();
}

void MediaHistory::cancelResume() {
    pendingResumeSeconds = -1;
}

void MediaHistory::setFinished(bool finished) {
    if (canSave())
        history->setFinished(QUrl::fromLocalFile(mediaPath), finished);
}

void MediaHistory::clear() {
    if (history)
        history->clear();
    rememberCurrent = false;
}
