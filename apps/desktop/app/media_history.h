#pragma once
#include "recent_videos.h"
#include "player_preferences.h"
#include <QPointer>
#include <QTimer>

// Owns checkpoint eligibility and pending resume state for the current media.
// The session supplies playback/track snapshots and performs the actual seek.
class MediaHistory : public QObject {
    Q_OBJECT
public:
    explicit MediaHistory(PlayerPreferences &preferences, QObject *parent = nullptr);
    void setHistory(RecentVideos *history);
    void beginMedia(const QString &path, bool local);
    std::optional<RecentVideo> savedVideo() const;
    double prepareResume(double duration);
    void recordOpened(RecentVideo snapshot);
    void checkpoint(RecentVideo snapshot);
    void observePosition(double seconds);
    void cancelResume();
    void setFinished(bool finished);
    void clear();
signals:
    void checkpointRequested();

private:
    bool canSave() const;
    PlayerPreferences &preferences;
    QPointer<RecentVideos> history;
    QTimer checkpointTimer;
    QString mediaPath;
    double pendingResumeSeconds = -1;
    bool ready = false;
    bool rememberCurrent = false;
};
