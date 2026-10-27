#include "learning_session.h"
#include "media_path.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <QStringDecoder>
#include <QScopedValueRollback>
#include <QDesktopServices>
#include <QRegularExpression>

LearningSession::LearningSession(PlaybackEngine *playback, QObject *parent)
    : QObject(parent), playerPreferences(playback, this), mediaHistory(playerPreferences, this),
      studyInteraction(*playback, subtitleTimeline, this),
      playbackEngine(playback) {
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
    connect(&mediaQueue, &MediaQueue::openRequested, this, &LearningSession::openVideo);
    connect(playbackEngine, &PlaybackEngine::metadataChanged, this, &LearningSession::mediaMetadataChanged);
    connect(playbackEngine, &PlaybackEngine::metadataChanged, this, &LearningSession::tracksChanged);
    connect(playbackEngine, &PlaybackEngine::stateChanged, this, &LearningSession::playbackStateChanged);
    connect(playbackEngine, &PlaybackEngine::finished, this, [this] {
        checkpoint();
        mediaHistory.setFinished(true);
        setPaused(true);
        if (mediaQueue.autoplay() && mediaQueue.hasNext())
            mediaQueue.next();
    });
    connect(playbackEngine, &PlaybackEngine::outputDeviceRemoved, this, [this] {
        if (mediaReady && !paused())
            setPaused(true);
        const auto selected = playerPreferences.audioDevice();
        bool available = selected == "auto";
        for (const auto &device : playbackEngine->audioDevices()) {
            if (device.toMap().value("name").toString() == selected)
                available = true;
        }
        if (!available)
            playerPreferences.setAudioDevice("auto");
    });
    connect(playbackEngine, &PlaybackEngine::operationFailed, this, [this](const QString &message) {
        statusText = message;
        emit statusChanged();
    });
    connect(playbackEngine, &PlaybackEngine::screenshotSaved, this, [this](const QString &) {
        statusText = "Screenshot saved";
        emit statusChanged();
    });
    connect(&mediaHistory, &MediaHistory::checkpointRequested, this, &LearningSession::checkpoint);
    connect(playbackEngine, &PlaybackEngine::failed, this, [this] {
        statusText = playbackEngine->error().isEmpty() ? "Cannot play this file." : playbackEngine->error();
        emit statusChanged();
    });
}
void LearningSession::setStudyServices(StudyServices *providedStudyServices) {
    studyServices = providedStudyServices;
    studyInteraction.setServices(providedStudyServices);
    connect(providedStudyServices->subtitleTools(), &SubtitleTools::trackReady, this,
            [this](const QString &path, bool secondary, bool preserveDelay) {
                QScopedValueRollback<bool> publishing(publishingSubtitle, true);
                if (preserveDelay)
                    embeddedTrackPaths.insert(secondary ? nativeSecondary : nativePrimary, path);
                else
                    setSubtitleDelay(0, secondary);
                if (secondary)
                    openTranslation(QUrl::fromLocalFile(path));
                else
                    openSubtitles(QUrl::fromLocalFile(path));
            });
    const auto updateSearchLanguage = [this, providedStudyServices] {
        const auto language = providedStudyServices->settings()->sourceLanguageCode();
        cueModel()->setSearchLanguage(language == "tr" || language == "az" || language == "azb"
                                         ? CueModel::Turkish : CueModel::Unicode);
    };
    connect(providedStudyServices->settings(), &AppSettings::changed, this, updateSearchLanguage);
    updateSearchLanguage();
    connect(providedStudyServices->subtitles(), &SubtitleService::subtitlesReady, this,
            [this](const QVector<Cue> &cues, const QString &path) {
                invalidateSubtitleTools();
                nativePrimary.clear();
                playbackEngine->configure(PlaybackEngine::Option::PrimarySubtitle, "no");
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
    mediaHistory.setHistory(videos);
}

QString LearningSession::mediaTitle() const {
    const QUrl url(currentMediaPath);
    return url.scheme().startsWith("http") ? url.fileName() : QFileInfo(currentMediaPath).completeBaseName();
}
QString LearningSession::sourceSubtitleName() const {
    if (!rememberedSourceSubtitlePath.isEmpty())
        return QFileInfo(rememberedSourceSubtitlePath).fileName();
    for (const auto &entry : playbackEngine->tracks()) {
        const auto track = entry.toMap();
        if (track.value("type") == "sub" && track.value("id").toString() == nativePrimary)
            return track.value("label").toString();
    }
    return subtitleTimeline.name(SubtitleRole::Primary);
}
QString LearningSession::translationSubtitleName() const {
    if (!rememberedTranslationSubtitlePath.isEmpty())
        return QFileInfo(rememberedTranslationSubtitlePath).fileName();
    for (const auto &entry : playbackEngine->tracks()) {
        const auto track = entry.toMap();
        if (track.value("type") == "sub" && track.value("id").toString() == nativeSecondary)
            return track.value("label").toString();
    }
    return subtitleTimeline.name(SubtitleRole::Secondary);
}

RecentVideo LearningSession::historySnapshot() const {
    const double seconds = pendingSubtitleSeekSeconds >= 0 ? pendingSubtitleSeekSeconds
                                                          : playbackEngine->snapshotPosition();
    RecentVideo video{currentMediaPath, seconds, qMax(0.0, duration()), rememberedSourceSubtitlePath,
                      rememberedTranslationSubtitlePath};
    video.primaryDelay = primaryDelaySeconds;
    video.secondaryDelay = secondaryDelaySeconds;
    video.primaryNativeTrack = nativePrimary;
    video.secondaryNativeTrack = nativeSecondary;
    return video;
}

void LearningSession::checkpoint() {
    mediaHistory.checkpoint(historySnapshot());
}

void LearningSession::finishOpening(const QString &path) {
    if (path != currentMediaPath)
        return;

    // 1. Restore each track independently before checkpoints can replace its saved association.
    playerPreferences.applyMedia();
    const auto saved = mediaHistory.savedVideo();
    const QString trackWarning = saved ? restoreSubtitleTracks(*saved) : QString();

    // 2. Restore the precise position and start playback even if a subtitle file is unavailable.
    mediaReady = true;
    const auto droppedSubtitles = std::exchange(pendingDroppedSubtitles, {});
    for (int index = 0; index < droppedSubtitles.size(); ++index) {
        if (index == 0)
            openSubtitles(droppedSubtitles[index]);
        else
            openTranslation(droppedSubtitles[index]);
    }
    const double resumePosition = mediaHistory.prepareResume(duration());
    if (resumePosition > 0)
        playbackEngine->seek(resumePosition);
    setPaused(false);
    statusText = trackWarning.isEmpty() ? "Ready to explore" : trackWarning;
    emit statusChanged();

    // 3. Promote only successful opens, retaining the target until playback reaches it.
    mediaHistory.recordOpened(historySnapshot());
}

QString LearningSession::restoreSubtitleTracks(const RecentVideo &saved) {
    QScopedValueRollback<bool> restoring(restoringSubtitleTracks, true);
    QStringList unavailable;
    setSubtitleDelay(saved.primaryDelay, false);
    setSubtitleDelay(saved.secondaryDelay, true);
    if (!saved.primaryNativeTrack.isEmpty())
        selectNativeSubtitle(saved.primaryNativeTrack, false);
    if (!saved.secondaryNativeTrack.isEmpty())
        selectNativeSubtitle(saved.secondaryNativeTrack, true);

    // 1. Keep unavailable associations so reconnecting a drive can restore them on the next open.
    if (rememberedSourceSubtitlePath.isEmpty() && !saved.primarySubtitlePath.isEmpty()) {
        rememberedSourceSubtitlePath = saved.primarySubtitlePath;
        openSubtitles(QUrl::fromLocalFile(saved.primarySubtitlePath));
        if (!QFileInfo(saved.primarySubtitlePath).isReadable())
            unavailable.append("primary");
    }
    if (rememberedTranslationSubtitlePath.isEmpty() && !saved.secondarySubtitlePath.isEmpty()) {
        rememberedTranslationSubtitlePath = saved.secondarySubtitlePath;
        openTranslation(QUrl::fromLocalFile(saved.secondarySubtitlePath));
        if (!QFileInfo(saved.secondarySubtitlePath).isReadable())
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
    if (loadNativeSubtitle(url, true))
        return;
    try {
        subtitleTimeline.load(url, SubtitleRole::Secondary, playerPreferences.subtitleEncoding());
        invalidateSubtitleTools();
        nativeSecondary.clear();
        playbackEngine->configure(PlaybackEngine::Option::SecondarySubtitle,
                                  "no"); // Validation completes before replacing the track.
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
    invalidateSubtitleTools();
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
    const bool remote =
        url.isValid() && (url.scheme() == "https" || url.scheme() == "http") && !url.host().isEmpty();
    if (!remote && (!url.isLocalFile() || !file.isFile() || !file.isReadable())) {
        statusText = "This video is unavailable. Choose an existing local video file.";
        emit statusChanged();
        return;
    }

    // 2. Checkpoint the old video before clearing its interaction and subtitle state.
    checkpoint();
    mediaReady = false;
    invalidateSubtitleTools();
    embeddedTrackPaths.clear();
    pendingDroppedSubtitles.clear();
    previousSeekPosition = scrubStart = -1;
    clearLoop();
    resetInteraction(true);
    setPaused(true);
    currentMediaPath = remote ? url.toString(QUrl::FullyEncoded) : mediaPathIdentity(file.absoluteFilePath());
    mediaHistory.beginMedia(currentMediaPath, !remote);
    mediaQueue.setCurrent(remote ? url : QUrl::fromLocalFile(currentMediaPath));
    playerPreferences.beginMedia(currentMediaPath);
    if (!playerPreferences.rememberSpeed())
        rate(1);
    nativePrimary.clear();
    nativeSecondary.clear();
    primaryDelaySeconds = secondaryDelaySeconds = 0;
    currentPrimaryCueIndex = -1;
    subtitleTimeline.reset();
    studyInteraction.setMedia(currentMediaPath);
    currentSecondaryCueIndex = -1;
    rememberedSourceSubtitlePath.clear();
    rememberedTranslationSubtitlePath.clear();
    statusText = remote ? "Opening network video…" : "Loading local video…";
    emit mediaChanged();
    emit tracksChanged();
    emit statusChanged();
    emit cueChanged();

    // 3. Bind study requests to this media before the adapter can finish loading it.
    if (studyServices && remote)
        studyServices->subtitles()->cancel();
    if (studyServices && !remote) {
        const auto saved = mediaHistory.savedVideo();
        studyServices->subtitles()->setMedia(currentMediaPath,
                                             !saved || saved->primarySubtitlePath.isEmpty());
    }
    playbackEngine->open(currentMediaPath, false);
}
void LearningSession::openSubtitles(const QUrl &url) {
    if (!url.isLocalFile())
        return;
    if (loadNativeSubtitle(url, false))
        return;
    try {
        subtitleTimeline.load(url, SubtitleRole::Primary, playerPreferences.subtitleEncoding());
        invalidateSubtitleTools();
        nativePrimary.clear();
        playbackEngine->configure(PlaybackEngine::Option::PrimarySubtitle, "no");
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
    if (playbackState() == "finished") {
        startOver();
        return;
    }
    setPaused(!paused());
}
void LearningSession::seek(double seconds) {
    if (!std::isfinite(seconds))
        return;
    previousSeekPosition = position();
    resetInteraction();
    mediaHistory.cancelResume();
    playbackEngine->seek(qBound(0.0, seconds, duration()));
}
void LearningSession::volume(double level) {
    if (!std::isfinite(level))
        return;

    volumePercent = qBound(0.0, level, 100.0);
    if (volumePercent > 0)
        unmutedVolumePercent = volumePercent;
    playbackEngine->volume(volumePercent);
    playerPreferences.saveVolume(volumePercent);
    emit volumeChanged();
}
void LearningSession::toggleMute() {
    volume(volumePercent > 0 ? 0 : unmutedVolumePercent);
}
void LearningSession::rate(double speed) {
    if (!std::isfinite(speed))
        return;
    speedMultiplier = qBound(0.25, speed, 3.0);
    playbackEngine->rate(speedMultiplier);
    playerPreferences.saveSpeed(speedMultiplier);
    emit speedChanged();
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
    mediaHistory.cancelResume();
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
    mediaHistory.cancelResume();
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
    const auto result = studyInteraction.saveCard();
    emit flashcardSaveFinished(result.success, result.message);
}
void LearningSession::syncPosition() {
    mediaHistory.observePosition(position());
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
    auto tracks = subtitleTimeline.tracks();
    for (const auto &entry : playbackEngine->tracks()) {
        const auto track = entry.toMap();
        if (track.value("type") == "sub") {
            const QString externalPath = mediaPathIdentity(track.value("external-filename").toString());
            const bool alreadyImported =
                track.value("external").toBool() &&
                std::any_of(tracks.cbegin(), tracks.cend(), [&externalPath](const QVariant &entry) {
                    return mediaPathIdentity(entry.toMap().value("path").toString()) == externalPath;
                });
            if (alreadyImported)
                continue;
            const QString id = track.value("id").toString();
            if (embeddedTrackPaths.contains(id)) {
                for (auto &entry : tracks) {
                    auto imported = entry.toMap();
                    if (imported.value("path").toString() == embeddedTrackPaths.value(id)) {
                        imported.insert("name", track.value("label"));
                        entry = imported;
                    }
                }
                continue;
            }
            tracks.append(QVariantMap{
                {"path", "embedded:" + id},
                {"name", track.value("label")},
                {"primary",
                 nativePrimary == id || (track.value("external").toBool() &&
                                         mediaPathIdentity(track.value("external-filename").toString()) ==
                                             rememberedSourceSubtitlePath)},
                {"secondary",
                 nativeSecondary == id || (track.value("external").toBool() &&
                                           mediaPathIdentity(track.value("external-filename").toString()) ==
                                               rememberedTranslationSubtitlePath)}});
        }
    }
    return tracks;
}

void LearningSession::chooseTrack(const QString &path, bool secondary) {
    if (path.startsWith("embedded:")) {
        selectNativeSubtitle(path.mid(9), secondary);
        return;
    }
    if (!subtitleTimeline.knows(path))
        return;

    if (secondary)
        openTranslation(QUrl::fromLocalFile(path));
    else
        openSubtitles(QUrl::fromLocalFile(path));
}

void LearningSession::clearTrack(bool secondary) {
    invalidateSubtitleTools();
    if (secondary)
        nativeSecondary.clear();
    else
        nativePrimary.clear();
    playbackEngine->configure(secondary ? PlaybackEngine::Option::SecondarySubtitle
                                        : PlaybackEngine::Option::PrimarySubtitle,
                              "no");
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
    applyNativeSubtitleTiming();
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

void LearningSession::commitSelection(bool secondary) {
    studyInteraction.commitSelection(secondary);
}
void LearningSession::explainCue(bool secondary) {
    studyInteraction.explainCue(secondary);
}
void LearningSession::invalidateStudy() {
    studyInteraction.dismiss();
}

void LearningSession::initializePreferences(const QString &directory) {
    playerPreferences.initialize(directory);
    volume(playerPreferences.savedVolume());
    rate(playerPreferences.savedSpeed());
}
void LearningSession::openVideos(const QList<QUrl> &urls) {
    mediaQueue.replace(urls);
    if (!mediaQueue.entries().isEmpty())
        mediaQueue.play(0);
}
void LearningSession::acceptDrop(const QList<QUrl> &urls) {
    QList<QUrl> videos, subtitles;
    for (const auto &url : urls) {
        const auto suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (QStringList{"srt", "vtt", "ass", "ssa", "sup", "sub", "idx"}.contains(suffix))
            subtitles << url;
        else
            videos << url;
    }
    if (!videos.isEmpty())
        openVideos(videos);
    if (!mediaReady && !videos.isEmpty()) {
        pendingDroppedSubtitles = subtitles.mid(0, 2);
        return;
    }
    // Drop loads primary then secondary; explicit dialogs remain available for replacement.
    for (int index = 0; index < subtitles.size() && index < 2; ++index) {
        if (index == 0)
            openSubtitles(subtitles[index]);
        else
            openTranslation(subtitles[index]);
    }
}
bool LearningSession::loadNativeSubtitle(const QUrl &url, bool secondary) {
    const auto suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
    if (!QStringList{"ass", "ssa", "sup", "sub", "idx"}.contains(suffix))
        return false;
    if (!QFileInfo(url.toLocalFile()).isReadable()) {
        statusText = "This subtitle file is unavailable.";
        emit statusChanged();
        return true;
    }
    clearTrack(secondary);
    if (secondary)
        rememberedTranslationSubtitlePath = url.toLocalFile();
    else
        rememberedSourceSubtitlePath = url.toLocalFile();
    playbackEngine->addSubtitle(url.toLocalFile(), secondary);
    applyNativeSubtitleTiming();
    statusText = "Styled or bitmap subtitles use the original track appearance.";
    emit tracksChanged();
    emit statusChanged();
    checkpoint();
    return true;
}
void LearningSession::selectNativeSubtitle(const QString &id, bool secondary) {
    bool found = id == "no";
    int streamIndex = -1;
    for (const auto &entry : playbackEngine->tracks()) {
        const auto track = entry.toMap();
        if (track.value("type") == "sub" && track.value("id").toString() == id) {
            if (track.value("external").toBool()) {
                const QUrl url = QUrl::fromLocalFile(track.value("external-filename").toString());
                if (secondary)
                    openTranslation(url);
                else
                    openSubtitles(url);
                return;
            }
            found = true;
            streamIndex = track.value("ff-index", -1).toInt();
        }
    }
    if (!found)
        return;
    clearTrack(secondary);
    if (id != "no") {
        if (secondary)
            nativeSecondary = id;
        else
            nativePrimary = id;
        playbackEngine->configure(secondary ? PlaybackEngine::Option::SecondarySubtitle
                                            : PlaybackEngine::Option::PrimarySubtitle,
                                  id);
    }
    applyNativeSubtitleTiming();
    emit tracksChanged();
    checkpoint();
    if (id != "no" && streamIndex >= 0 && studyServices)
        studyServices->subtitleTools()->extract(currentMediaPath, streamIndex, secondary);
}
void LearningSession::setSubtitleDelay(double seconds, bool secondary) {
    if (!std::isfinite(seconds))
        return;
    invalidateSubtitleTools();
    seconds = qBound(-600.0, seconds, 600.0);
    if (secondary)
        secondaryDelaySeconds = seconds;
    else
        primaryDelaySeconds = seconds;
    stopReplay();
    if (!independentNativeSubtitleTiming() && usesNativeSubtitles(false) && usesNativeSubtitles(true)) {
        primaryDelaySeconds = secondaryDelaySeconds = seconds;
    }
    subtitleTimeline.setDelay(qRound64(seconds * 1000),
                              secondary ? SubtitleRole::Secondary : SubtitleRole::Primary);
    applyNativeSubtitleTiming();
    syncPosition();
    emit cueChanged();
    emit tracksChanged();
    checkpoint();
}
void LearningSession::previewSeek(double seconds) {
    if (!std::isfinite(seconds) || !seekable())
        return;
    if (scrubStart < 0) {
        scrubStart = position();
        resetInteraction();
    }
    playbackEngine->previewSeek(seconds);
}
void LearningSession::finishSeek(double seconds) {
    const double origin = scrubStart >= 0 ? scrubStart : position();
    seek(seconds);
    previousSeekPosition = origin;
    scrubStart = -1;
}
void LearningSession::undoSeek() {
    if (previousSeekPosition < 0)
        return;
    const double target = previousSeekPosition;
    seek(target);
}
void LearningSession::startOver() {
    mediaHistory.setFinished(false);
    seek(0);
    setPaused(false);
}
void LearningSession::frameStep(bool forward) {
    resetInteraction();
    setPaused(true);
    playbackEngine->execute(forward ? PlaybackEngine::Command::FrameForward
                                    : PlaybackEngine::Command::FrameBackward);
}
QString LearningSession::loopLabel() const {
    return loopStart < 0 ? "Set loop start" : loopEnd < 0 ? "Set loop end" : "Clear A–B loop";
}
void LearningSession::cycleLoop() {
    if (!seekable())
        return;
    if (loopStart < 0) {
        loopStart = position();
        playbackEngine->configure(PlaybackEngine::Option::LoopStart, loopStart);
    } else if (loopEnd < 0 && position() > loopStart) {
        loopEnd = position();
        playbackEngine->configure(PlaybackEngine::Option::LoopEnd, loopEnd);
    } else
        clearLoop();
    emit loopChanged();
}
void LearningSession::clearLoop() {
    loopStart = loopEnd = -1;
    playbackEngine->configure(PlaybackEngine::Option::LoopStart, "no");
    playbackEngine->configure(PlaybackEngine::Option::LoopEnd, "no");
    emit loopChanged();
}
void LearningSession::jumpToChapter(int index) {
    const auto chapters = playbackEngine->chapters();
    if (index >= 0 && index < chapters.size())
        seek(chapters[index].toMap().value("time").toDouble());
}
void LearningSession::chapterStep(int direction) {
    const auto chapters = playbackEngine->chapters();
    if (direction > 0) {
        for (const auto &chapter : chapters) {
            const double time = chapter.toMap().value("time").toDouble();
            if (time > position() + 0.1) {
                seek(time);
                return;
            }
        }
    } else {
        for (auto chapter = chapters.crbegin(); chapter != chapters.crend(); ++chapter) {
            const double time = chapter->toMap().value("time").toDouble();
            if (time < position() - 3) {
                seek(time);
                return;
            }
        }
        seek(0);
    }
}
bool LearningSession::jumpToTime(const QString &time) {
    static const QRegularExpression pattern("^(?:(\\d+):)?([0-5]?\\d):([0-5]\\d(?:\\.\\d+)?)$");
    const auto match = pattern.match(time.trimmed());
    bool valid = false;
    double seconds = time.trimmed().toDouble(&valid);
    if (match.hasMatch()) {
        seconds = match.captured(1).toDouble() * 3600 + match.captured(2).toDouble() * 60 +
                  match.captured(3).toDouble();
        valid = true;
    }
    if (!valid || !std::isfinite(seconds) || seconds < 0 || !seekable())
        return false;
    seek(seconds);
    return true;
}
void LearningSession::revealFile() {
    const QFileInfo file(currentMediaPath);
    if (file.isAbsolute() && file.exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
}
void LearningSession::clearHistory() {
    mediaHistory.clear();
    playerPreferences.clearMedia();
}
void LearningSession::saveScreenshot(const QUrl &url) {
    if (url.isLocalFile())
        playbackEngine->screenshot(url.toLocalFile(), false);
}

bool LearningSession::usesNativeSubtitles(bool secondary) const {
    const auto native = secondary ? nativeSecondary : nativePrimary;
    const auto path = secondary ? rememberedTranslationSubtitlePath : rememberedSourceSubtitlePath;
    return !native.isEmpty() ||
           QStringList{"ass", "ssa", "sup", "sub", "idx"}.contains(QFileInfo(path).suffix().toLower());
}
void LearningSession::applyNativeSubtitleTiming() {
    if (independentNativeSubtitleTiming()) {
        playbackEngine->configure(PlaybackEngine::Option::PrimarySubtitleDelay, primaryDelaySeconds);
        playbackEngine->configure(PlaybackEngine::Option::SecondarySubtitleDelay, secondaryDelaySeconds);
    } else {
        // Older mpv has one delay for both native tracks. QML text timelines remain independent.
        const double delay = usesNativeSubtitles(false) ? primaryDelaySeconds : secondaryDelaySeconds;
        if (usesNativeSubtitles(false) && usesNativeSubtitles(true))
            secondaryDelaySeconds = primaryDelaySeconds;
        playbackEngine->configure(PlaybackEngine::Option::PrimarySubtitleDelay, delay);
    }
}

void LearningSession::invalidateSubtitleTools() {
    if (studyServices && !publishingSubtitle && !restoringSubtitleTracks)
        studyServices->subtitleTools()->invalidate();
}
void LearningSession::prepareSubtitleTools(bool secondary) {
    if (!studyServices)
        return;
    studyServices->subtitleTools()->prepare(
        subtitleTimeline.snapshot(secondary ? SubtitleRole::Secondary : SubtitleRole::Primary),
        subtitleTimeline.snapshot(secondary ? SubtitleRole::Primary : SubtitleRole::Secondary), secondary);
}
