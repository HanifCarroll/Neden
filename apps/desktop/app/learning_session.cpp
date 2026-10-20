#include "learning_session.h"
#include "media_path.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <QStringDecoder>

LearningSession::LearningSession(PlaybackEngine *playback, QObject *parent)
    : QObject(parent), interaction(*playback, timeline, this), engine(playback) {
    connect(engine, &PlaybackEngine::positionChanged, this, &LearningSession::positionChanged);
    connect(engine, &PlaybackEngine::durationChanged, this, &LearningSession::durationChanged);
    connect(engine, &PlaybackEngine::pausedChanged, this, &LearningSession::pausedChanged);
    connect(engine, &PlaybackEngine::positionChanged, this, &LearningSession::syncPosition);
    connect(engine, &PlaybackEngine::loaded, this, &LearningSession::finishOpening);
    connect(&interaction, &StudyInteraction::selectionChanged, this, &LearningSession::selectionChanged);
    connect(&interaction, &StudyInteraction::started, this, &LearningSession::stopReplay);
    connect(&interaction, &StudyInteraction::explanationRequested, this,
            &LearningSession::explanationRequested);
    connect(&interaction, &StudyInteraction::error, this, [this](const QString &message) {
        statusText = message;
        emit statusChanged();
    });
    checkpointTimer.setInterval(5000);
    connect(&checkpointTimer, &QTimer::timeout, this, &LearningSession::checkpoint);
    checkpointTimer.start();
    connect(engine, &PlaybackEngine::failed, this, [this] {
        statusText = "Cannot play this file.";
        emit statusChanged();
    });
}
void LearningSession::setStudyServices(StudyServices *tools) {
    services = tools;
    interaction.setServices(tools);
    connect(tools->subtitles(), &SubtitleService::subtitlesReady, this,
            [this](const QVector<Cue> &cues, const QString &path) {
                stopReplay();
                resetInteraction(true);
                timeline.publish(cues, path, SubtitleRole::Primary);
                rememberedSourceSubtitlePath = path;
                currentCue = -1;
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
    return timeline.name(SubtitleRole::Primary);
}
QString LearningSession::translationSubtitleName() const {
    return timeline.name(SubtitleRole::Secondary);
}

void LearningSession::checkpoint() {
    if (!recentVideos || !mediaReady || !rememberCurrent)
        return;

    // 1. Save the current viewing position, preserving a pending resume target.
    const double seconds = restorePosition >= 0      ? restorePosition
                           : subtitleSeekTarget >= 0 ? subtitleSeekTarget
                                                     : engine->snapshotPosition();
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
    restorePosition = saved ? saved->position : -1;
    if (restorePosition > 0) {
        if (duration() > 0)
            restorePosition = qMin(restorePosition, duration());
        engine->seek(restorePosition);
    } else {
        restorePosition = -1;
    }
    setPaused(false);
    statusText = trackWarning.isEmpty() ? "Ready to explore" : trackWarning;
    emit statusChanged();

    // 3. Promote only successful opens, retaining the target until playback reaches it.
    if (recentVideos && rememberCurrent) {
        recentVideos->remember({currentMediaPath, restorePosition >= 0 ? restorePosition : position(),
                                qMax(0.0, duration()), rememberedSourceSubtitlePath,
                                rememberedTranslationSubtitlePath},
                               true);
    }
}

QString LearningSession::restoreSubtitleTracks(const RecentVideo &saved) {
    QStringList unavailable;

    // 1. Keep unavailable associations so reconnecting a drive can restore them on the next open.
    if (rememberedSourceSubtitlePath.isEmpty() && !saved.primarySubtitlePath.isEmpty()) {
        rememberedSourceSubtitlePath = saved.primarySubtitlePath;
        openSubtitles(QUrl::fromLocalFile(saved.primarySubtitlePath));
        if (timeline.path(SubtitleRole::Primary).isEmpty())
            unavailable.append("primary");
    }
    if (rememberedTranslationSubtitlePath.isEmpty() && !saved.secondarySubtitlePath.isEmpty()) {
        rememberedTranslationSubtitlePath = saved.secondarySubtitlePath;
        openTranslation(QUrl::fromLocalFile(saved.secondarySubtitlePath));
        if (timeline.path(SubtitleRole::Secondary).isEmpty())
            unavailable.append("secondary");
    }

    // 2. A failed track must not prevent the other track or the video from loading.
    if (unavailable.isEmpty())
        return {};

    return "Could not reload the saved " + unavailable.join(" and ") +
           " subtitles. Reconnect their drive or choose replacement files above the transcript.";
}

void LearningSession::explainSelection() {
    interaction.explainSelection();
}

bool LearningSession::followUp(const QString &question) {
    return interaction.followUp(question);
}

void LearningSession::replayExplainedCue() {
    if (replayActive) {
        stopReplay();
        return;
    }
    if (services && services->explanation()->target()) {
        const auto &target = *services->explanation()->target();
        playSpan(target.startMs / 1000.0, target.endMs / 1000.0);
    }
}

void LearningSession::openTranslation(const QUrl &url) {
    if (!url.isLocalFile())
        return;
    try {
        timeline.load(url, SubtitleRole::Secondary); // Validation completes before replacing the track.
        stopReplay();
        closeDefinition();
        resetInteraction(true);
        translationCue = timeline.active(qRound64(position() * 1000), SubtitleRole::Secondary);
        rememberedTranslationSubtitlePath = timeline.path(SubtitleRole::Secondary);
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
    engine->setPaused(value);
}
void LearningSession::replaceCues(QVector<Cue> next) {
    stopReplay();
    resetInteraction(true);
    if (services)
        services->subtitles()->cancel();
    currentCue = -1;
    timeline.replacePrimary(std::move(next));
    syncPosition();
    emit cueChanged();
}
void LearningSession::resetInteraction(bool clearConversation) {
    interaction.reset(clearConversation);
    replayActive = false;
    replayArmed = false;
    subtitleSeekTarget = -1;
    subtitleSeekIndex = -1;
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
    restorePosition = -1;
    rememberCurrent = true;
    resetInteraction(true);
    setPaused(true);
    currentMediaPath = mediaPathIdentity(file.absoluteFilePath());
    currentCue = -1;
    timeline.reset();
    interaction.setMedia(currentMediaPath);
    translationCue = -1;
    rememberedSourceSubtitlePath.clear();
    rememberedTranslationSubtitlePath.clear();
    statusText = "Loading local video…";
    emit mediaChanged();
    emit tracksChanged();
    emit statusChanged();
    emit cueChanged();

    // 3. Bind study requests to this media before the adapter can finish loading it.
    if (services) {
        const auto saved = recentVideos ? recentVideos->find(currentMediaPath) : std::nullopt;
        services->subtitles()->setMedia(currentMediaPath, !saved || saved->primarySubtitlePath.isEmpty());
    }
    engine->open(currentMediaPath, false);
}
void LearningSession::openSubtitles(const QUrl &url) {
    if (!url.isLocalFile())
        return;
    try {
        timeline.load(url, SubtitleRole::Primary);
        stopReplay();
        closeDefinition();
        resetInteraction(true);
        if (services)
            services->subtitles()->cancel();
        currentCue = -1;
        syncPosition();
        emit cueChanged();
        rememberedSourceSubtitlePath = timeline.path(SubtitleRole::Primary);
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
    restorePosition = -1;
    engine->seek(qBound(0.0, seconds, duration()));
}
void LearningSession::volume(double level) {
    if (!std::isfinite(level))
        return;

    audioLevel = qBound(0.0, level, 100.0);
    if (audioLevel > 0)
        unmutedLevel = audioLevel;
    engine->volume(audioLevel);
    emit volumeChanged();
}
void LearningSession::toggleMute() {
    volume(audioLevel > 0 ? 0 : unmutedLevel);
}
void LearningSession::rate(double speed) {
    engine->rate(speed);
}
QString LearningSession::subtitle() const {
    return timeline.text(qRound64(position() * 1000), SubtitleRole::Primary);
}
QString LearningSession::translation() const {
    return timeline.text(qRound64(position() * 1000), SubtitleRole::Secondary);
}
void LearningSession::select(const QString &text, bool secondary) {
    interaction.select(text, secondary);
}
void LearningSession::closeDefinition() {
    interaction.closeDefinition();
}
int LearningSession::previousSubtitleIndex() const {
    return timeline.previous(qRound64(position() * 1000), subtitleSeekIndex);
}

int LearningSession::nextSubtitleIndex() const {
    return timeline.next(qRound64(position() * 1000), subtitleSeekIndex);
}

void LearningSession::seekSubtitle(double seconds) {
    if (!std::isfinite(seconds))
        return;

    // Pause before seeking so neither an asynchronous seek nor closing a definition resumes playback.
    resetInteraction();
    restorePosition = -1;
    setPaused(true);
    subtitleSeekTarget = qBound(0.0, seconds, duration());
    subtitleSeekIndex = timeline.active(qRound64(subtitleSeekTarget * 1000));
    engine->seek(subtitleSeekTarget);
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
    const int cueIndex = subtitleSeekIndex >= 0 ? subtitleSeekIndex : currentCue;
    playSubtitle(cueIndex);
}

void LearningSession::playSubtitle(int index) {
    const auto *cue = timeline.cue(index);
    if (!cue)
        return;
    playSpan(cue->startMs / 1000.0, cue->endMs / 1000.0);
}

void LearningSession::playSpan(double start, double end) {
    resetInteraction();
    restorePosition = -1;
    setPaused(true);
    replayStart = start;
    replayEnd = end;
    replayArmed = false;
    replayActive = true;
    subtitleSeekTarget = start;
    subtitleSeekIndex = timeline.active(qRound64(start * 1000));
    emit replayingChanged();
    engine->seek(start);
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
    engine->seek(qMax(replayStart, replayEnd - 0.050));
}

void LearningSession::saveCard() {
    interaction.saveCard();
}
void LearningSession::syncPosition() {
    if (restorePosition >= 0 && std::abs(position() - restorePosition) < 1)
        restorePosition = -1;
    if (subtitleSeekTarget >= 0 && std::abs(position() - subtitleSeekTarget) < 0.05) {
        subtitleSeekTarget = -1;
        subtitleSeekIndex = -1;
    }

    // 1. Notify subtitle views only when their cue actually changes.
    const int translated = timeline.active(qRound64(position() * 1000), SubtitleRole::Secondary);
    if (translated != translationCue) {
        translationCue = translated;
        interaction.cueChanged();
        emit cueChanged();
    }
    const int next = timeline.active(qRound64(position() * 1000));
    if (next != currentCue) {
        currentCue = next;
        interaction.cueChanged();
        emit cueChanged();
    }

    // 2. Finish replay only after its requested start has been observed.
    if (replayActive) {
        if (position() >= replayStart - 0.1 && position() < replayEnd) {
            replayArmed = true;
            subtitleSeekTarget = -1;
            subtitleSeekIndex = -1;
        }
        if (replayArmed && position() >= replayEnd)
            finishReplay();
    }
}

QVariantList LearningSession::subtitleTracks() const {
    return timeline.tracks();
}

void LearningSession::chooseTrack(const QString &path, bool secondary) {
    if (!timeline.knows(path))
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
    timeline.clear(secondary ? SubtitleRole::Secondary : SubtitleRole::Primary);
    if (secondary) {
        translationCue = -1;
        rememberedTranslationSubtitlePath.clear();
    } else {
        currentCue = -1;
        rememberedSourceSubtitlePath.clear();
    }
    emit tracksChanged();
    emit cueChanged();
    checkpoint();
}

void LearningSession::lookupWordAt(const QString &text, int offset, bool secondary) {
    interaction.lookupWordAt(text, offset, secondary);
}

void LearningSession::closeWordCard(bool resume) {
    interaction.closeWordCard(resume);
}

void LearningSession::retryWordLookup() {
    interaction.retryWordLookup();
}

void LearningSession::translateWord() {
    interaction.translateWord();
}

void LearningSession::commitSelection(bool secondary) {
    interaction.commitSelection(secondary);
}
void LearningSession::explainCue(bool secondary) {
    interaction.explainCue(secondary);
}
void LearningSession::explainWord() {
    interaction.explainWord();
}
void LearningSession::invalidateStudy() {
    interaction.dismiss();
}
