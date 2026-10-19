#include "learning_session.h"
#include "media_path.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <QStringDecoder>

LearningSession::LearningSession(PlaybackEngine *playback, QObject *parent) : QObject(parent), engine(playback) {
    connect(engine, &PlaybackEngine::positionChanged, this, &LearningSession::positionChanged);
    connect(engine, &PlaybackEngine::durationChanged, this, &LearningSession::durationChanged);
    connect(engine, &PlaybackEngine::pausedChanged, this, &LearningSession::pausedChanged);
    connect(engine, &PlaybackEngine::positionChanged, this, &LearningSession::syncPosition);
    connect(engine, &PlaybackEngine::loaded, this, &LearningSession::finishOpening);
    checkpointTimer.setInterval(5000);
    connect(&checkpointTimer, &QTimer::timeout, this, &LearningSession::checkpoint);
    checkpointTimer.start();
    connect(engine, &PlaybackEngine::failed, this, [this] {
        statusText = "Cannot play this file.";
        emit statusChanged();
    });
}
void LearningSession::setStudyTools(StudyTools *tools) {
    studyTools = tools;
    connect(tools, &StudyTools::subtitlesReady, this, [this](const QVector<Cue> &cues) {
        stopReplay();
        closeDefinition();
        replaceCues(cues);
        sourceSubtitlePath.clear();
        emit tracksChanged();
        checkpoint();
    });
}

LearningSession::~LearningSession() { checkpoint(); }

void LearningSession::setRecentVideos(RecentVideos *videos) {
    recentVideos = videos;
    connect(videos, &RecentVideos::removed, this, [this](const QString &path) {
        if (currentMediaPath == path) rememberCurrent = false;
    });
}

QString LearningSession::mediaTitle() const { return QFileInfo(currentMediaPath).completeBaseName(); }
QString LearningSession::sourceSubtitleName() const { return QFileInfo(sourceSubtitlePath).fileName(); }
QString LearningSession::translationSubtitleName() const { return QFileInfo(translationSubtitlePath).fileName(); }

void LearningSession::checkpoint() {
    if (!recentVideos || !mediaReady || !rememberCurrent) return;

    // 1. Save the current viewing position, preserving a pending resume target.
    const double seconds = restorePosition >= 0 ? restorePosition
        : subtitleSeekTarget >= 0 ? subtitleSeekTarget : engine->snapshotPosition();
    if (!std::isfinite(seconds) || seconds < 0) return;

    const RecentVideo video{currentMediaPath, seconds, qMax(0.0, duration())};
    const auto previous = recentVideos->find(currentMediaPath);
    if (previous && previous->position == video.position && previous->duration == video.duration) return;

    // 2. Commit only changed positions to local history.
    recentVideos->remember(video);
}

void LearningSession::finishOpening(const QString &path) {
    if (path != currentMediaPath) return;

    // 1. Restore only playback position; subtitle choices remain session-only.
    const auto saved = recentVideos ? recentVideos->find(path) : std::nullopt;
    mediaReady = true;
    restorePosition = saved ? saved->position : -1;
    if (restorePosition > 0) {
        if (duration() > 0) restorePosition = qMin(restorePosition, duration());
        engine->seek(restorePosition);
    } else {
        restorePosition = -1;
    }
    setPaused(false);
    statusText = "Ready to explore";
    emit statusChanged();

    // 2. Promote only successful opens, retaining the target until playback reaches it.
    if (recentVideos && rememberCurrent) {
        recentVideos->remember({currentMediaPath,
            restorePosition >= 0 ? restorePosition : position(), qMax(0.0, duration())}, true);
    }
}

void LearningSession::explainSelection(bool translate) {
    if (studyTools) studyTools->explain(selectedText.isEmpty() ? subtitle() : selectedText, subtitle(), translate);
}

void LearningSession::openTranslation(const QUrl &url) {
    if (!url.isLocalFile()) return;
    QFile file(url.toLocalFile());
    try {
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
            throw std::runtime_error("Cannot read translation file (8 MiB maximum).");
        }
        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString text = decoder(file.readAll());
        if (decoder.hasError()) throw std::runtime_error("Save the translation subtitles as UTF-8 SRT.");
        translationCues = parseSrt(text);
        model.setTranslationCues(translationCues);
        translationCue = cueAt(translationCues, qRound64(position() * 1000));
        if (studyTools) studyTools->invalidateContext();
        translationSubtitlePath = QFileInfo(file).absoluteFilePath();
        statusText = "Secondary subtitles loaded";
        emit cueChanged();
        emit tracksChanged();
        checkpoint();
    } catch (const std::exception &error) { statusText = QString::fromUtf8(error.what()); }
    emit statusChanged();
}

void LearningSession::setPaused(bool value) { engine->setPaused(value); }
void LearningSession::replaceCues(QVector<Cue> next) {
    stopReplay();
    resetInteraction();
    if (studyTools) studyTools->invalidateTrack();
    currentCue = -1;
    model.replace(std::move(next));
    model.setTranslationCues(translationCues);
    syncPosition();
    emit cueChanged();
}
void LearningSession::resetInteraction() {
    if (studyTools) studyTools->invalidateContext();
    replayActive = false;
    subtitleSeekTarget = -1;
    subtitleSeekIndex = -1;
    selectedText.clear();
    resumeAfterDefinition = false;
    emit selectionChanged();
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
    resetInteraction();
    setPaused(true);
    currentMediaPath = mediaPathIdentity(file.absoluteFilePath());
    currentCue = -1;
    model.replace({});
    translationCues.clear();
    translationCue = -1;
    sourceSubtitlePath.clear();
    translationSubtitlePath.clear();
    statusText = "Loading local video…";
    emit mediaChanged();
    emit tracksChanged();
    emit statusChanged();
    emit cueChanged();

    // 3. Bind study requests to this media before the adapter can finish loading it.
    if (studyTools) studyTools->setMedia(currentMediaPath);
    engine->open(currentMediaPath, false);
}
void LearningSession::openSubtitles(const QUrl &url) {

    // 1. Validate and parse a bounded local file before changing session state.
    if (!url.isLocalFile()) {
        return;
    }

    try {
        QFile file(url.toLocalFile());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
            throw std::runtime_error("Cannot read subtitle file (8 MiB maximum).");
        }

        QStringDecoder decoder(QStringDecoder::Utf8);
        const QString text = decoder.decode(file.readAll());
        if (decoder.hasError()) throw std::runtime_error("Subtitles must use UTF-8 encoding.");
        const auto next = parseSrt(text);

        // 2. End the previous interaction, then publish the replacement track.
        stopReplay();
        closeDefinition();
        currentCue = -1;
        replaceCues(next);
        sourceSubtitlePath = QFileInfo(file).absoluteFilePath();
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
    if (!std::isfinite(level)) return;

    audioLevel = qBound(0.0, level, 100.0);
    if (audioLevel > 0) unmutedLevel = audioLevel;
    engine->volume(audioLevel);
    emit volumeChanged();
}
void LearningSession::toggleMute() {
    volume(audioLevel > 0 ? 0 : unmutedLevel);
}
void LearningSession::rate(double speed) {
    engine->rate(speed);
}
QString LearningSession::subtitle() const { return currentCue >= 0 ? model.cues[currentCue].text : QString(); }
QString LearningSession::translation() const {
    if (!translationCues.isEmpty()) return translationCue >= 0 ? translationCues[translationCue].text : QString();
    return currentCue >= 0 ? model.cues[currentCue].translation : QString();
}
void LearningSession::select(const QString &text) {
    if (text.trimmed().isEmpty() || text.trimmed() == selectedText) {
        return;
    }

    if (replayActive) {
        stopReplay();
    }
    if (selectedText.isEmpty()) {
        resumeAfterDefinition = !paused();
    }
    if (studyTools) studyTools->invalidateContext();
    selectedText = text.trimmed();
    emit selectionChanged();
    setPaused(true);
}
void LearningSession::closeDefinition() {
    if (selectedText.isEmpty()) {
        return;
    }

    if (studyTools) studyTools->invalidateContext();
    selectedText.clear();
    const bool resume = resumeAfterDefinition;
    resumeAfterDefinition = false;
    if (resume) {
        setPaused(false);
    }
    emit selectionChanged();
}
int LearningSession::previousSubtitleIndex() const {
    if (subtitleSeekIndex >= 0) return subtitleSeekIndex - 1;

    const auto milliseconds = qRound64(position() * 1000);
    const auto active = cueAt(model.cues, milliseconds);
    if (active >= 0) return active - 1;

    const auto next = std::lower_bound(model.cues.begin(), model.cues.end(), milliseconds,
        [](const Cue &cue, qint64 time) { return cue.startMs < time; });
    return next == model.cues.begin() ? -1 : int(next - model.cues.begin()) - 1;
}

int LearningSession::nextSubtitleIndex() const {
    if (subtitleSeekIndex >= 0) return subtitleSeekIndex + 1 < model.cues.size() ? subtitleSeekIndex + 1 : -1;

    const auto milliseconds = qRound64(position() * 1000);
    const auto next = std::upper_bound(model.cues.begin(), model.cues.end(), milliseconds,
        [](qint64 time, const Cue &cue) { return time < cue.startMs; });
    return next == model.cues.end() ? -1 : int(next - model.cues.begin());
}

void LearningSession::seekSubtitle(double seconds) {
    if (!std::isfinite(seconds)) return;

    // Pause before seeking so neither an asynchronous seek nor closing a definition resumes playback.
    resetInteraction();
    restorePosition = -1;
    setPaused(true);
    subtitleSeekTarget = qBound(0.0, seconds, duration());
    subtitleSeekIndex = cueAt(model.cues, qRound64(subtitleSeekTarget * 1000));
    engine->seek(subtitleSeekTarget);
    emit cueChanged();
}

void LearningSession::previousSubtitle() {
    const int index = previousSubtitleIndex();
    if (index >= 0) seekSubtitle(model.cues[index].startMs / 1000.0);
}

void LearningSession::nextSubtitle() {
    const int index = nextSubtitleIndex();
    if (index >= 0) seekSubtitle(model.cues[index].startMs / 1000.0);
}

void LearningSession::replay() {

    // 1. A second press stops at the current position, still paused.
    if (replayActive) {
        stopReplay();
        return;
    }
    const int cueIndex = subtitleSeekIndex >= 0 ? subtitleSeekIndex : currentCue;
    if (cueIndex < 0) return;

    // 2. Freeze the chosen cue before clearing any definition or prior interaction.
    const Cue cue = model.cues[cueIndex];
    resetInteraction();
    restorePosition = -1;
    setPaused(true);
    replayStart = cue.startMs / 1000.0;
    replayEnd = cue.endMs / 1000.0;
    replayArmed = false;
    replayActive = true;
    emit replayingChanged();
    engine->seek(replayStart);
    setPaused(false);
}

void LearningSession::stopReplay() {
    if (!replayActive) return;

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
    if (studyTools) {
        studyTools->saveCard(selectedText.isEmpty() ? subtitle() : selectedText, subtitle(), translation(), position());
    }
}
void LearningSession::syncPosition() {
    if (restorePosition >= 0 && std::abs(position() - restorePosition) < 1) restorePosition = -1;
    if (subtitleSeekTarget >= 0 && std::abs(position() - subtitleSeekTarget) < 0.05) {
        subtitleSeekTarget = -1;
        subtitleSeekIndex = -1;
    }

    // 1. Notify subtitle views only when their cue actually changes.
    const int translated = cueAt(translationCues, qRound64(position() * 1000));
    if (translated != translationCue) {
        translationCue = translated;
        emit cueChanged();
    }
    const int next = cueAt(model.cues, qRound64(position() * 1000));
    if (next != currentCue) {
        currentCue = next;
        emit cueChanged();
    }

    // 2. Finish replay only after its requested start has been observed.
    if (replayActive) {
        if (position() >= replayStart - 0.1 && position() < replayEnd) replayArmed = true;
        if (replayArmed && position() >= replayEnd) finishReplay();
    }
}
