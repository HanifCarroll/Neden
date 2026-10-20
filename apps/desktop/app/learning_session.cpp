#include "learning_session.h"
#include "media_path.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <QStringDecoder>

LearningSession::LearningSession(PlaybackEngine *playback, QObject *parent)
    : QObject(parent), studyInteraction(*playback, subtitleTimeline, this), playbackEngine(playback) {
    connect(playbackEngine, &PlaybackEngine::positionChanged, this, &LearningSession::positionChanged);
    connect(playbackEngine, &PlaybackEngine::durationChanged, this, &LearningSession::durationChanged);
    connect(playbackEngine, &PlaybackEngine::pausedChanged, this, &LearningSession::pausedChanged);
    connect(playbackEngine, &PlaybackEngine::positionChanged, this, &LearningSession::syncPosition);
    connect(playbackEngine, &PlaybackEngine::loaded, this, &LearningSession::finishOpening);
    connect(&studyInteraction, &StudyInteraction::selectionChanged, this, &LearningSession::selectionChanged);
    connect(&studyInteraction, &StudyInteraction::started, this, &LearningSession::stopReplay);
    connect(&studyInteraction, &StudyInteraction::explanationRequested, this,
            &LearningSession::explanationRequested);
    connect(&studyInteraction, &StudyInteraction::error, this, [this](const QString &message) {
        statusText = message;
        emit statusChanged();
    });
    checkpointTimer.setInterval(5000);
    connect(&checkpointTimer, &QTimer::timeout, this, &LearningSession::checkpoint);
    checkpointTimer.start();
    connect(playbackEngine, &PlaybackEngine::failed, this, [this] {
        statusText = "Cannot play this file.";
        emit statusChanged();
    });
}
void LearningSession::setStudyServices(StudyServices *providedStudyServices) {
    studyServices = providedStudyServices;
    studyInteraction.setServices(providedStudyServices);
    connect(providedStudyServices->subtitles(), &SubtitleService::subtitlesReady, this,
            [this](const QVector<Cue> &cues, const QString &path) {
                stopReplay();
                resetInteraction(true);
                subtitleTimeline.publish(cues, path, SubtitleRole::Primary);
                rememberedSourceSubtitlePath = path;
                currentPrimaryCueIndex = -1;
                syncPosition();
                emit tracksChanged();
                checkpoint();
            });
}

LearningSession::~LearningSession() {
    checkpoint();
}

void LearningSession::setRecentVideos(RecentVideos *videos) {
    recentVideos = videos;
    connect(videos, &RecentVideos::removed, this, [this](const QString &path) {
        if (currentMediaPath == path)
            rememberCurrent = false;
    });
}

QString LearningSession::mediaTitle() const {
    return QFileInfo(currentMediaPath).completeBaseName();
}
QString LearningSession::sourceSubtitleName() const {
    return subtitleTimeline.name(SubtitleRole::Primary);
}
QString LearningSession::translationSubtitleName() const {
    return subtitleTimeline.name(SubtitleRole::Secondary);
}

void LearningSession::checkpoint() {
    if (!recentVideos || !mediaReady || !rememberCurrent)
        return;

    // 1. Save the current viewing position, preserving a pending resume target.
    const double seconds = pendingResumeSeconds >= 0         ? pendingResumeSeconds
                           : pendingSubtitleSeekSeconds >= 0 ? pendingSubtitleSeekSeconds
                                                             : playbackEngine->snapshotPosition();
    if (!std::isfinite(seconds) || seconds < 0)
        return;

    const RecentVideo video{currentMediaPath, seconds, qMax(0.0, duration()), rememberedSourceSubtitlePath,
                            rememberedTranslationSubtitlePath};
    const auto previous = recentVideos->find(currentMediaPath);
    if (previous && previous->position == video.position && previous->duration == video.duration &&
        previous->primarySubtitlePath == video.primarySubtitlePath &&
        previous->secondarySubtitlePath == video.secondarySubtitlePath)
        return;

    // 2. Commit only changed positions or track associations to local history.
    recentVideos->remember(video);
}

void LearningSession::finishOpening(const QString &path) {
    if (path != currentMediaPath)
        return;

    // 1. Restore each track independently before checkpoints can replace its saved association.
    const auto saved = recentVideos ? recentVideos->find(path) : std::nullopt;
    const QString trackWarning = saved ? restoreSubtitleTracks(*saved) : QString();

    // 2. Restore the precise position and start playback even if a subtitle file is unavailable.
    mediaReady = true;
    pendingResumeSeconds = saved ? saved->position : -1;
    if (pendingResumeSeconds > 0) {
        if (duration() > 0)
            pendingResumeSeconds = qMin(pendingResumeSeconds, duration());
        playbackEngine->seek(pendingResumeSeconds);
    } else {
        pendingResumeSeconds = -1;
    }
    setPaused(false);
    statusText = trackWarning.isEmpty() ? "Ready to explore" : trackWarning;
    emit statusChanged();

    // 3. Promote only successful opens, retaining the target until playback reaches it.
    if (recentVideos && rememberCurrent) {
        recentVideos->remember(
            {currentMediaPath, pendingResumeSeconds >= 0 ? pendingResumeSeconds : position(),
             qMax(0.0, duration()), rememberedSourceSubtitlePath, rememberedTranslationSubtitlePath},
            true);
    }
}

QString LearningSession::restoreSubtitleTracks(const RecentVideo &saved) {
    QStringList unavailable;

    // 1. Keep unavailable associations so reconnecting a drive can restore them on the next open.
    if (rememberedSourceSubtitlePath.isEmpty() && !saved.primarySubtitlePath.isEmpty()) {
        rememberedSourceSubtitlePath = saved.primarySubtitlePath;
        openSubtitles(QUrl::fromLocalFile(saved.primarySubtitlePath));
        if (subtitleTimeline.path(SubtitleRole::Primary).isEmpty())
            unavailable.append("primary");
    }
    if (rememberedTranslationSubtitlePath.isEmpty() && !saved.secondarySubtitlePath.isEmpty()) {
        rememberedTranslationSubtitlePath = saved.secondarySubtitlePath;
        openTranslation(QUrl::fromLocalFile(saved.secondarySubtitlePath));
        if (subtitleTimeline.path(SubtitleRole::Secondary).isEmpty())
            unavailable.append("secondary");
    }

    // 2. A failed track must not prevent the other track or the video from loading.
    if (unavailable.isEmpty())
        return {};

    return "Could not reload the saved " + unavailable.join(" and ") +
           " subtitles. Reconnect their drive or choose replacement files above the transcript.";
}

void LearningSession::explainSelection() {
    studyInteraction.explainSelection();
}

bool LearningSession::followUp(const QString &question) {
    return studyInteraction.followUp(question);
}

void LearningSession::replayExplainedCue() {
    if (replayActive) {
        stopReplay();
        return;
    }
    if (studyServices && studyServices->explanation()->target()) {
        const auto &target = *studyServices->explanation()->target();
        playSpan(target.startMs / 1000.0, target.endMs / 1000.0);
    }
}

void LearningSession::openTranslation(const QUrl &url) {
    if (!url.isLocalFile())
        return;
    try {
        subtitleTimeline.load(url,
                              SubtitleRole::Secondary); // Validation completes before replacing the track.
        stopReplay();
        closeDefinition();
        resetInteraction(true);
        currentSecondaryCueIndex =
            subtitleTimeline.active(qRound64(position() * 1000), SubtitleRole::Secondary);
        rememberedTranslationSubtitlePath = subtitleTimeline.path(SubtitleRole::Secondary);
        statusText = "Secondary subtitles loaded";
        emit cueChanged();
        emit tracksChanged();
        checkpoint();
    } catch (const std::exception &error) {
        statusText = QString::fromUtf8(error.what());
    }
    emit statusChanged();
}

void LearningSession::setPaused(bool value) {
    playbackEngine->setPaused(value);
}
void LearningSession::replaceCues(QVector<Cue> next) {
    stopReplay();
    resetInteraction(true);
    if (studyServices)
        studyServices->subtitles()->cancel();
    currentPrimaryCueIndex = -1;
    subtitleTimeline.replacePrimary(std::move(next));
    syncPosition();
    emit cueChanged();
}
void LearningSession::resetInteraction(bool clearConversation) {
    studyInteraction.reset(clearConversation);
    replayActive = false;
    replayArmed = false;
    pendingSubtitleSeekSeconds = -1;
    pendingSubtitleCueIndex = -1;
    emit replayingChanged();
}
void LearningSession::openVideo(const QUrl &url) {

    // 1. Validate the local media before replacing the current playable session.
    const QFileInfo file(url.toLocalFile());
    if (!url.isLocalFile() || !file.isFile() || !file.isReadable()) {
        statusText = "This video is unavailable. Choose an existing local video file.";
        emit statusChanged();
        return;
    }

    // 2. Checkpoint the old video before clearing its interaction and subtitle state.
    checkpoint();
    mediaReady = false;
    pendingResumeSeconds = -1;
    rememberCurrent = true;
    resetInteraction(true);
    setPaused(true);
    currentMediaPath = mediaPathIdentity(file.absoluteFilePath());
    currentPrimaryCueIndex = -1;
    subtitleTimeline.reset();
    studyInteraction.setMedia(currentMediaPath);
    currentSecondaryCueIndex = -1;
    rememberedSourceSubtitlePath.clear();
    rememberedTranslationSubtitlePath.clear();
    statusText = "Loading local video…";
    emit mediaChanged();
    emit tracksChanged();
    emit statusChanged();
    emit cueChanged();

    // 3. Bind study requests to this media before the adapter can finish loading it.
    if (studyServices) {
        const auto saved = recentVideos ? recentVideos->find(currentMediaPath) : std::nullopt;
        studyServices->subtitles()->setMedia(currentMediaPath,
                                             !saved || saved->primarySubtitlePath.isEmpty());
    }
    playbackEngine->open(currentMediaPath, false);
}
void LearningSession::openSubtitles(const QUrl &url) {
    if (!url.isLocalFile())
        return;
    try {
        subtitleTimeline.load(url, SubtitleRole::Primary);
        stopReplay();
        closeDefinition();
        resetInteraction(true);
        if (studyServices)
            studyServices->subtitles()->cancel();
        currentPrimaryCueIndex = -1;
        syncPosition();
        emit cueChanged();
        rememberedSourceSubtitlePath = subtitleTimeline.path(SubtitleRole::Primary);
        statusText = "Primary subtitles loaded";
        emit tracksChanged();
        checkpoint();
    } catch (const std::exception &error) {
        statusText = QString::fromUtf8(error.what());
    }
    emit statusChanged();
}
void LearningSession::toggle() {
    if (replayActive) {
        stopReplay();
        return;
    }

    resetInteraction();
    setPaused(!paused());
}
void LearningSession::seek(double seconds) {
    resetInteraction();
    pendingResumeSeconds = -1;
    playbackEngine->seek(qBound(0.0, seconds, duration()));
}
void LearningSession::volume(double level) {
    if (!std::isfinite(level))
        return;

    volumePercent = qBound(0.0, level, 100.0);
    if (volumePercent > 0)
        unmutedVolumePercent = volumePercent;
    playbackEngine->volume(volumePercent);
    emit volumeChanged();
}
void LearningSession::toggleMute() {
    volume(volumePercent > 0 ? 0 : unmutedVolumePercent);
}
void LearningSession::rate(double speed) {
    playbackEngine->rate(speed);
}
QString LearningSession::subtitle() const {
    return subtitleTimeline.text(qRound64(position() * 1000), SubtitleRole::Primary);
}
QString LearningSession::translation() const {
    return subtitleTimeline.text(qRound64(position() * 1000), SubtitleRole::Secondary);
}
void LearningSession::select(const QString &text, bool secondary) {
    studyInteraction.select(text, secondary);
}
void LearningSession::closeDefinition() {
    studyInteraction.closeDefinition();
}
int LearningSession::previousSubtitleIndex() const {
    return subtitleTimeline.previous(qRound64(position() * 1000), pendingSubtitleCueIndex);
}

int LearningSession::nextSubtitleIndex() const {
    return subtitleTimeline.next(qRound64(position() * 1000), pendingSubtitleCueIndex);
}

void LearningSession::seekSubtitle(double seconds) {
    if (!std::isfinite(seconds))
        return;

    // Pause before seeking so neither an asynchronous seek nor closing a definition resumes playback.
    resetInteraction();
    pendingResumeSeconds = -1;
    setPaused(true);
    pendingSubtitleSeekSeconds = qBound(0.0, seconds, duration());
    pendingSubtitleCueIndex = subtitleTimeline.active(qRound64(pendingSubtitleSeekSeconds * 1000));
    playbackEngine->seek(pendingSubtitleSeekSeconds);
    emit cueChanged();
}

void LearningSession::previousSubtitle() {
    playSubtitle(previousSubtitleIndex());
}

void LearningSession::nextSubtitle() {
    playSubtitle(nextSubtitleIndex());
}

void LearningSession::replay() {

    // 1. A second press stops at the current position, still paused.
    if (replayActive) {
        stopReplay();
        return;
    }
    const int cueIndex = pendingSubtitleCueIndex >= 0 ? pendingSubtitleCueIndex : currentPrimaryCueIndex;
    playSubtitle(cueIndex);
}

void LearningSession::playSubtitle(int index) {
    const auto *cue = subtitleTimeline.cue(index);
    if (!cue)
        return;
    playSpan(cue->startMs / 1000.0, cue->endMs / 1000.0);
}

void LearningSession::playSpan(double start, double end) {
    resetInteraction();
    pendingResumeSeconds = -1;
    setPaused(true);
    replayStartSeconds = start;
    replayEndSeconds = end;
    replayArmed = false;
    replayActive = true;
    pendingSubtitleSeekSeconds = start;
    pendingSubtitleCueIndex = subtitleTimeline.active(qRound64(start * 1000));
    emit replayingChanged();
    playbackEngine->seek(start);
    setPaused(false);
    emit cueChanged();
}

void LearningSession::stopReplay() {
    if (!replayActive)
        return;

    replayActive = false;
    replayArmed = false;
    setPaused(true);
    emit replayingChanged();
}

void LearningSession::finishReplay() {
    stopReplay();
    // Keep the final subtitle visible for study rather than advancing into the next cue or gap.
    playbackEngine->seek(qMax(replayStartSeconds, replayEndSeconds - 0.050));
}

void LearningSession::saveCard() {
    studyInteraction.saveCard();
}
void LearningSession::syncPosition() {
    if (pendingResumeSeconds >= 0 && std::abs(position() - pendingResumeSeconds) < 1)
        pendingResumeSeconds = -1;
    if (pendingSubtitleSeekSeconds >= 0 && std::abs(position() - pendingSubtitleSeekSeconds) < 0.05) {
        pendingSubtitleSeekSeconds = -1;
        pendingSubtitleCueIndex = -1;
    }

    // 1. Notify subtitle views only when their cue actually changes.
    const int translated = subtitleTimeline.active(qRound64(position() * 1000), SubtitleRole::Secondary);
    if (translated != currentSecondaryCueIndex) {
        currentSecondaryCueIndex = translated;
        studyInteraction.cueChanged();
        emit cueChanged();
    }
    const int next = subtitleTimeline.active(qRound64(position() * 1000));
    if (next != currentPrimaryCueIndex) {
        currentPrimaryCueIndex = next;
        studyInteraction.cueChanged();
        emit cueChanged();
    }

    // 2. Finish replay only after its requested start has been observed.
    if (replayActive) {
        if (position() >= replayStartSeconds - 0.1 && position() < replayEndSeconds) {
            replayArmed = true;
            pendingSubtitleSeekSeconds = -1;
            pendingSubtitleCueIndex = -1;
        }
        if (replayArmed && position() >= replayEndSeconds)
            finishReplay();
    }
}

QVariantList LearningSession::subtitleTracks() const {
    return subtitleTimeline.tracks();
}

void LearningSession::chooseTrack(const QString &path, bool secondary) {
    if (!subtitleTimeline.knows(path))
        return;

    if (secondary)
        openTranslation(QUrl::fromLocalFile(path));
    else
        openSubtitles(QUrl::fromLocalFile(path));
}

void LearningSession::clearTrack(bool secondary) {
    closeWordCard();
    closeDefinition();
    resetInteraction(true);
    subtitleTimeline.clear(secondary ? SubtitleRole::Secondary : SubtitleRole::Primary);
    if (secondary) {
        currentSecondaryCueIndex = -1;
        rememberedTranslationSubtitlePath.clear();
    } else {
        currentPrimaryCueIndex = -1;
        rememberedSourceSubtitlePath.clear();
    }
    emit tracksChanged();
    emit cueChanged();
    checkpoint();
}

void LearningSession::lookupWordAt(const QString &text, int offset, bool secondary) {
    studyInteraction.lookupWordAt(text, offset, secondary);
}

void LearningSession::closeWordCard(bool resume) {
    studyInteraction.closeWordCard(resume);
}

void LearningSession::retryWordLookup() {
    studyInteraction.retryWordLookup();
}

void LearningSession::translateWord() {
    studyInteraction.translateWord();
}

void LearningSession::commitSelection(bool secondary) {
    studyInteraction.commitSelection(secondary);
}
void LearningSession::explainCue(bool secondary) {
    studyInteraction.explainCue(secondary);
}
void LearningSession::explainWord() {
    studyInteraction.explainWord();
}
void LearningSession::invalidateStudy() {
    studyInteraction.dismiss();
}
