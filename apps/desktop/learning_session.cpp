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
    connect(tools, &StudyTools::subtitlesReady, this, [this](const QVector<Cue> &cues, const QString &path) {
        stopReplay();
        closeDefinition();
        replaceCues(cues);
        sourceSubtitlePath = path;
        if (!loadedSubtitlePaths.contains(path)) loadedSubtitlePaths.append(path);
        rememberedSourceSubtitlePath = path;
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

    const RecentVideo video{currentMediaPath, seconds, qMax(0.0, duration()),
        rememberedSourceSubtitlePath, rememberedTranslationSubtitlePath};
    const auto previous = recentVideos->find(currentMediaPath);
    if (previous && previous->position == video.position && previous->duration == video.duration
        && previous->primarySubtitlePath == video.primarySubtitlePath
        && previous->secondarySubtitlePath == video.secondarySubtitlePath) return;

    // 2. Commit only changed positions or track associations to local history.
    recentVideos->remember(video);
}

void LearningSession::finishOpening(const QString &path) {
    if (path != currentMediaPath) return;

    // 1. Restore each track independently before checkpoints can replace its saved association.
    const auto saved = recentVideos ? recentVideos->find(path) : std::nullopt;
    const QString trackWarning = saved ? restoreSubtitleTracks(*saved) : QString();

    // 2. Restore the precise position and start playback even if a subtitle file is unavailable.
    mediaReady = true;
    restorePosition = saved ? saved->position : -1;
    if (restorePosition > 0) {
        if (duration() > 0) restorePosition = qMin(restorePosition, duration());
        engine->seek(restorePosition);
    } else {
        restorePosition = -1;
    }
    setPaused(false);
    statusText = trackWarning.isEmpty() ? "Ready to explore" : trackWarning;
    emit statusChanged();

    // 3. Promote only successful opens, retaining the target until playback reaches it.
    if (recentVideos && rememberCurrent) {
        recentVideos->remember({currentMediaPath,
            restorePosition >= 0 ? restorePosition : position(), qMax(0.0, duration()),
            rememberedSourceSubtitlePath, rememberedTranslationSubtitlePath}, true);
    }
}

QString LearningSession::restoreSubtitleTracks(const RecentVideo &saved) {
    QStringList unavailable;

    // 1. Keep unavailable associations so reconnecting a drive can restore them on the next open.
    if (rememberedSourceSubtitlePath.isEmpty() && !saved.primarySubtitlePath.isEmpty()) {
        rememberedSourceSubtitlePath = saved.primarySubtitlePath;
        openSubtitles(QUrl::fromLocalFile(saved.primarySubtitlePath));
        if (sourceSubtitlePath.isEmpty()) unavailable.append("primary");
    }
    if (rememberedTranslationSubtitlePath.isEmpty() && !saved.secondarySubtitlePath.isEmpty()) {
        rememberedTranslationSubtitlePath = saved.secondarySubtitlePath;
        openTranslation(QUrl::fromLocalFile(saved.secondarySubtitlePath));
        if (translationSubtitlePath.isEmpty()) unavailable.append("secondary");
    }

    // 2. A failed track must not prevent the other track or the video from loading.
    if (unavailable.isEmpty()) return {};

    return "Could not reload the saved " + unavailable.join(" and ")
        + " subtitles. Reconnect their drive or choose replacement files above the transcript.";
}

QJsonObject LearningSession::subtitleContext() const {
    if (!studyTools || currentCue < 0) return {};
    const Cue &cue = model.cues[currentCue];
    QJsonArray before;
    QJsonArray after;
    QJsonArray secondary;
    for (int index = qMax(0, currentCue - 3); index < currentCue; ++index) before.append(model.cues[index].text);
    for (int index = currentCue + 1; index < qMin(int(model.cues.size()), currentCue + 4); ++index) after.append(model.cues[index].text);
    if (studyTools->includeSecondary()) {
        for (const auto &translation : translationCues) {
            if (translation.startMs < cue.endMs && translation.endMs > cue.startMs) secondary.append(translation.text);
        }
    }

    return {{"sourceCue", cue.text}, {"before", before}, {"after", after}, {"secondary", secondary},
        {"sourceLanguage", studyTools->sourceLanguage()}, {"explanationLanguage", studyTools->explanationLanguage()},
        {"cueIndex", currentCue}, {"cueStartMs", cue.startMs}, {"cueEndMs", cue.endMs}};
}

void LearningSession::explainSelection() {
    if (!studyTools || currentCue < 0) return;
    const Cue &cue = model.cues[currentCue];
    if (selectedText.isEmpty()) select(cue.text);
    auto context = subtitleContext();
    context.insert("selection", selectedText.isEmpty() ? cue.text : selectedText);
    explainedCueIndex = currentCue;
    studyTools->explainContext(context);
}

bool LearningSession::followUp(const QString &question) {
    if (!studyTools || studyTools->subject().isEmpty()) return false;

    select(studyTools->subject());
    const bool sent = studyTools->followUp(question);
    if (sent) setPaused(true);
    return sent;
}

void LearningSession::replayExplainedCue() {
    if (replayActive) { stopReplay(); return; }

    playSubtitle(explainedCueIndex);
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
        if (decoder.hasError()) throw std::runtime_error("Save the translation subtitles as UTF-8 SRT or WebVTT.");
        const auto next = parseSrt(text);
        closeWordCard();
        closeDefinition();
        resetInteraction(true);
        translationCues = next;
        model.setTranslationCues(translationCues);
        translationCue = cueAt(translationCues, qRound64(position() * 1000));
        if (studyTools) studyTools->invalidateContext();
        translationSubtitlePath = QFileInfo(file).absoluteFilePath();
        if (!loadedSubtitlePaths.contains(translationSubtitlePath)) loadedSubtitlePaths.append(translationSubtitlePath);
        rememberedTranslationSubtitlePath = translationSubtitlePath;
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
    resetInteraction(true);
    if (studyTools) studyTools->invalidateTrack();
    currentCue = -1;
    model.replace(std::move(next));
    model.setTranslationCues(translationCues);
    syncPosition();
    emit cueChanged();
}
void LearningSession::resetInteraction(bool clearConversation) {
    closeWordCard(false);
    if (studyTools) {
        if (clearConversation) studyTools->invalidateContext();
        else studyTools->stop();
    }
    if (clearConversation) explainedCueIndex = -1;
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
    resetInteraction(true);
    setPaused(true);
    currentMediaPath = mediaPathIdentity(file.absoluteFilePath());
    currentCue = -1;
    model.replace({});
    translationCues.clear();
    translationCue = -1;
    loadedSubtitlePaths.clear();
    sourceSubtitlePath.clear();
    translationSubtitlePath.clear();
    rememberedSourceSubtitlePath.clear();
    rememberedTranslationSubtitlePath.clear();
    statusText = "Loading local video…";
    emit mediaChanged();
    emit tracksChanged();
    emit statusChanged();
    emit cueChanged();

    // 3. Bind study requests to this media before the adapter can finish loading it.
    if (studyTools) {
        const auto saved = recentVideos ? recentVideos->find(currentMediaPath) : std::nullopt;
        studyTools->setMedia(currentMediaPath, !saved || saved->primarySubtitlePath.isEmpty());
    }
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
        if (!loadedSubtitlePaths.contains(sourceSubtitlePath)) loadedSubtitlePaths.append(sourceSubtitlePath);
        rememberedSourceSubtitlePath = sourceSubtitlePath;
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
        resumeAfterDefinition = wordActive ? wordResume : !paused();
    }
    closeWordCard(false);
    if (studyTools) studyTools->stop();
    selectedText = text.trimmed();
    emit selectionChanged();
    setPaused(true);
}
void LearningSession::closeDefinition() {
    if (studyTools) studyTools->stop();
    if (selectedText.isEmpty()) {
        return;
    }

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
    if (index < 0 || index >= model.cues.size()) return;

    // 1. Freeze the chosen cue before clearing any definition or prior interaction.
    const Cue cue = model.cues[index];
    resetInteraction();
    restorePosition = -1;
    setPaused(true);
    replayStart = cue.startMs / 1000.0;
    replayEnd = cue.endMs / 1000.0;
    replayArmed = false;
    replayActive = true;
    subtitleSeekTarget = replayStart;
    subtitleSeekIndex = index;
    emit replayingChanged();

    // 2. Play only this cue; syncPosition pauses at its end after the seek arrives.
    engine->seek(replayStart);
    setPaused(false);
    emit cueChanged();
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
        const auto front = studyTools->subject().isEmpty() ? (selectedText.isEmpty() ? subtitle() : selectedText) : studyTools->subject();
        studyTools->saveCard(front, subtitle(), translation(), position());
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
        closeWordCard(false);
        emit cueChanged();
    }
    const int next = cueAt(model.cues, qRound64(position() * 1000));
    if (next != currentCue) {
        currentCue = next;
        closeWordCard(false);
        emit cueChanged();
    }

    // 2. Finish replay only after its requested start has been observed.
    if (replayActive) {
        if (position() >= replayStart - 0.1 && position() < replayEnd) {
            replayArmed = true;
            subtitleSeekTarget = -1;
            subtitleSeekIndex = -1;
        }
        if (replayArmed && position() >= replayEnd) finishReplay();
    }
}

QVariantList LearningSession::subtitleTracks() const {
    QVariantList tracks;
    for (const auto &path : loadedSubtitlePaths) {
        tracks.append(QVariantMap{{"path", path}, {"name", QFileInfo(path).fileName()},
            {"primary", path == sourceSubtitlePath}, {"secondary", path == translationSubtitlePath}});
    }
    return tracks;
}

void LearningSession::chooseTrack(const QString &path, bool secondary) {
    if (!loadedSubtitlePaths.contains(path)) return;

    if (secondary) openTranslation(QUrl::fromLocalFile(path));
    else openSubtitles(QUrl::fromLocalFile(path));
}

void LearningSession::clearTrack(bool secondary) {
    closeWordCard();
    closeDefinition();
    resetInteraction(true);
    if (secondary) {
        translationCues.clear();
        translationCue = -1;
        translationSubtitlePath.clear();
        rememberedTranslationSubtitlePath.clear();
        model.setTranslationCues({});
    } else {
        model.replace({});
        currentCue = -1;
        sourceSubtitlePath.clear();
        rememberedSourceSubtitlePath.clear();
    }
    emit tracksChanged();
    emit cueChanged();
    checkpoint();
}

void LearningSession::lookupWordAt(const QString &text, int offset) {
    if (!studyTools || studyTools->learningSettings().value("wordProvider").toString() == "off") return;

    const QString word = WordLookup::wordAt(text, offset);
    if (word.isEmpty()) return;

    const bool resume = wordActive ? wordResume : !paused();
    closeWordCard(false);
    wordResume = resume;
    wordActive = true;
    wordContext = subtitleContext();
    wordContext.insert("clickedSubtitle", text);
    wordContext.insert("explanationLanguage", studyTools->explanationLanguage());
    wordContext.insert("sourceLanguage", studyTools->sourceLanguage());
    setPaused(true);
    studyTools->lookupWord(word, wordContext);
}

void LearningSession::closeWordCard(bool resume) {
    if (!wordActive) return;

    const bool shouldResume = resume && wordResume;
    wordActive = false;
    wordContext = {};
    wordResume = false;
    if (studyTools) studyTools->words()->cancel();
    if (shouldResume) setPaused(false);
}

void LearningSession::retryWordLookup() {
    if (!wordActive || !studyTools || studyTools->words()->state() != "failed") return;
    studyTools->lookupWord(studyTools->words()->word(), wordContext);
}

void LearningSession::translateWord() {
    if (!wordActive || !studyTools) return;

    const auto word = studyTools->words()->word();
    studyTools->words()->lookup(word, studyTools->learningSettings(), true);
}
