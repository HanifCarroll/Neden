#include "learning_session.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>
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

    // 1. Save the viewing position rather than a temporary sentence replay position.
    const double seconds = replayActive ? returnPosition
        : restorePosition >= 0 ? restorePosition : engine->snapshotPosition();
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
    if (studyTools) studyTools->invalidateTrack();
    currentCue = -1;
    model.replace(std::move(next));
    syncPosition();
    emit cueChanged();
}
void LearningSession::resetInteraction() {
    if (studyTools) studyTools->invalidateContext();
    replayActive = false;
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
    currentMediaPath = file.absoluteFilePath();
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
    engine->volume(level);
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
    if (text.trimmed().isEmpty()) {
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
void LearningSession::replay() {

    // 1. Require an active cue and treat a second replay command as Stop.
    if (currentCue < 0) {
        return;
    }

    if (replayActive) {
        stopReplay();
        return;
    }

    // 2. Save the return state before starting the sentence.
    returnPosition = position();
    returnPaused = paused();
    replayStart = model.cues[currentCue].startMs / 1000.0;
    replayEnd = model.cues[currentCue].endMs / 1000.0;
    replayArmed = false;
    replayActive = true;
    emit replayingChanged();
    engine->seek(replayStart);
    setPaused(false);
}
void LearningSession::stopReplay() {
    if (!replayActive) {
        return;
    }

    replayActive = false;
    emit replayingChanged();
    engine->seek(returnPosition);
    setPaused(returnPaused);
}
void LearningSession::saveCard() {
    if (studyTools) {
        studyTools->saveCard(selectedText.isEmpty() ? subtitle() : selectedText, subtitle(), translation(), position());
    }
}
void LearningSession::syncPosition() {
    if (restorePosition >= 0 && std::abs(position() - restorePosition) < 1) restorePosition = -1;

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
        if (replayArmed && position() >= replayEnd) stopReplay();
    }
}
