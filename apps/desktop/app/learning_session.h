#pragma once
#include "cues.h"
#include "study_services.h"
#include "study_interaction.h"
#include "subtitle_timeline.h"
#include "media_history.h"
#include "media_queue.h"
#include "player_preferences.h"
#include <QUrl>
#include "playback_engine.h"

class LearningSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Session is supplied by the application")
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(double volumeLevel READ volumeLevel WRITE volume NOTIFY volumeChanged)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY cueChanged)
    Q_PROPERTY(QString translation READ translation NOTIFY cueChanged)
    Q_PROPERTY(QString selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString mediaFile READ mediaPath NOTIFY mediaChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaChanged)
    Q_PROPERTY(QString sourceSubtitleName READ sourceSubtitleName NOTIFY tracksChanged)
    Q_PROPERTY(QString translationSubtitleName READ translationSubtitleName NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY tracksChanged)
    Q_PROPERTY(bool replaying READ replaying NOTIFY replayingChanged)
    Q_PROPERTY(int activeCue READ activeCue NOTIFY cueChanged)
    Q_PROPERTY(bool canPreviousSubtitle READ canPreviousSubtitle NOTIFY cueChanged)
    Q_PROPERTY(bool canNextSubtitle READ canNextSubtitle NOTIFY cueChanged)
    Q_PROPERTY(MediaQueue *queue READ queue CONSTANT)
    Q_PROPERTY(PlayerPreferences *preferences READ preferences CONSTANT)
    Q_PROPERTY(bool independentNativeSubtitleTiming READ independentNativeSubtitleTiming CONSTANT)
    Q_PROPERTY(QVariantList mediaTracks READ mediaTracks NOTIFY mediaMetadataChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY mediaMetadataChanged)
    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY mediaMetadataChanged)
    Q_PROPERTY(QVariantMap mediaInformation READ mediaInformation NOTIFY mediaMetadataChanged)
    Q_PROPERTY(QString playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY mediaMetadataChanged)
    Q_PROPERTY(double playbackSpeed READ playbackSpeed NOTIFY speedChanged)
    Q_PROPERTY(double primaryDelay READ primaryDelay NOTIFY tracksChanged)
    Q_PROPERTY(double secondaryDelay READ secondaryDelay NOTIFY tracksChanged)
    Q_PROPERTY(QString primaryNativeTrack READ primaryNativeTrack NOTIFY tracksChanged)
    Q_PROPERTY(QString secondaryNativeTrack READ secondaryNativeTrack NOTIFY tracksChanged)
    Q_PROPERTY(QString loopLabel READ loopLabel NOTIFY loopChanged)
public:
    MediaQueue *queue() { return &mediaQueue; }
    PlayerPreferences *preferences() { return &playerPreferences; }
    bool independentNativeSubtitleTiming() const {
        return playbackEngine->supportsIndependentNativeSubtitleTiming();
    }
    QVariantList mediaTracks() const { return playbackEngine->tracks(); }
    QVariantList chapters() const { return playbackEngine->chapters(); }
    QVariantList audioDevices() const { return playbackEngine->audioDevices(); }
    QVariantMap mediaInformation() const { return playbackEngine->information(); }
    QString playbackState() const { return playbackEngine->state(); }
    bool seekable() const { return playbackEngine->seekable(); }
    double playbackSpeed() const { return speedMultiplier; }
    double primaryDelay() const { return primaryDelaySeconds; }
    double secondaryDelay() const { return secondaryDelaySeconds; }
    QString primaryNativeTrack() const { return nativePrimary; }
    QString secondaryNativeTrack() const { return nativeSecondary; }
    QString loopLabel() const;
    void initializePreferences(const QString &directory);
    QString mediaPath() const { return currentMediaPath; }
    Q_INVOKABLE void openVideos(const QList<QUrl> &urls);
    Q_INVOKABLE void acceptDrop(const QList<QUrl> &urls);
    Q_INVOKABLE void selectNativeSubtitle(const QString &id, bool secondary);
    Q_INVOKABLE void setSubtitleDelay(double seconds, bool secondary);
    Q_INVOKABLE void previewSeek(double seconds);
    Q_INVOKABLE void finishSeek(double seconds);
    Q_INVOKABLE void undoSeek();
    Q_INVOKABLE void startOver();
    Q_INVOKABLE void frameStep(bool forward);
    Q_INVOKABLE void cycleLoop();
    Q_INVOKABLE void clearLoop();
    Q_INVOKABLE void jumpToChapter(int index);
    Q_INVOKABLE void chapterStep(int direction);
    Q_INVOKABLE bool jumpToTime(const QString &time);
    Q_INVOKABLE void revealFile();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE QString localFilePath(const QUrl &url) const {
        return url.isLocalFile() ? url.toLocalFile() : QString();
    }
    Q_INVOKABLE void saveScreenshot(const QUrl &url);
    Q_PROPERTY(StudyServices *study READ study CONSTANT)
    StudyServices *study() const { return studyServices; }
    void setStudyServices(StudyServices *providedStudyServices);
    void setRecentVideos(RecentVideos *videos);
    ~LearningSession() override;
    bool hasMedia() const { return !currentMediaPath.isEmpty(); }
    QString mediaTitle() const;
    QString sourceSubtitleName() const;
    QString translationSubtitleName() const;
    QVariantList subtitleTracks() const;
    Q_INVOKABLE void chooseTrack(const QString &path, bool secondary);
    Q_INVOKABLE void clearTrack(bool secondary);
    Q_INVOKABLE void prepareSubtitleTools(bool secondary);
    Q_INVOKABLE void checkpoint();
    explicit LearningSession(PlaybackEngine *playbackEngine, QObject *parent = nullptr);
    Q_PROPERTY(CueModel *cueModel READ cueModel CONSTANT)
    CueModel *cueModel() { return subtitleTimeline.transcript(); }
    void replaceCues(QVector<Cue> next);
    void setPaused(bool value);
    double position() const { return playbackEngine->position(); }
    double duration() const { return playbackEngine->duration(); }
    bool paused() const { return playbackEngine->paused(); }
    QString subtitle() const;
    QString translation() const;
    int translationCueCount() const { return subtitleTimeline.count(SubtitleRole::Secondary); }
    QString selection() const { return studyInteraction.selection(); }
    QString status() const { return statusText; }
    bool replaying() const { return replayActive; }
    int activeCue() const { return currentPrimaryCueIndex; }
    Q_INVOKABLE void openVideo(const QUrl &url);
    Q_INVOKABLE void openTranslation(const QUrl &url);
    Q_INVOKABLE void explainSelection();
    Q_INVOKABLE void commitSelection(bool secondary = false);
    Q_INVOKABLE void explainCue(bool secondary = false);
    Q_INVOKABLE void invalidateStudy();
    Q_INVOKABLE void lookupWordAt(const QString &text, int offset, bool secondary = false, int requestDelayMs = 0);
    Q_INVOKABLE void retryWordLookup();
    Q_INVOKABLE void closeWordCard(bool resume = true);
    Q_INVOKABLE bool followUp(const QString &question);
    Q_INVOKABLE void replayExplainedCue();
    Q_INVOKABLE void openSubtitles(const QUrl &url);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void setPlaying() {
        resetInteraction();
        if (playbackState() == "finished")
            startOver();
        else
            setPaused(false);
    }
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void rate(double speed);
    double volumeLevel() const { return volumePercent; }
    Q_INVOKABLE void volume(double level);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void select(const QString &text, bool secondary = false);
    Q_INVOKABLE void closeDefinition();
    bool canPreviousSubtitle() const { return previousSubtitleIndex() >= 0; }
    bool canNextSubtitle() const { return nextSubtitleIndex() >= 0; }
    Q_INVOKABLE void previousSubtitle();
    Q_INVOKABLE void nextSubtitle();
    Q_INVOKABLE void seekSubtitle(double seconds);
    Q_INVOKABLE void replay();
    Q_INVOKABLE void stopReplay();
    Q_INVOKABLE void saveCard();
signals:
    void flashcardSaveFinished(bool success, QString message);
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void cueChanged();
    void selectionChanged();
    void statusChanged();
    void replayingChanged();
    void mediaChanged();
    void tracksChanged();
    void volumeChanged();
    void explanationRequested();
    void mediaMetadataChanged();
    void playbackStateChanged();
    void speedChanged();
    void loopChanged();

private:
    RecentVideo historySnapshot() const;
    void playSpan(double start, double end);
    int previousSubtitleIndex() const;
    int nextSubtitleIndex() const;
    void playSubtitle(int index);
    void finishReplay();
    void syncPosition();
    void resetInteraction(bool clearConversation = false);
    void finishOpening(const QString &path);
    QString restoreSubtitleTracks(const RecentVideo &saved);
    bool loadNativeSubtitle(const QUrl &url, bool secondary);
    bool usesNativeSubtitles(bool secondary) const;
    void applyNativeSubtitleTiming();
    MediaQueue mediaQueue;
    PlayerPreferences playerPreferences;
    MediaHistory mediaHistory;
    double speedMultiplier = 1;
    double primaryDelaySeconds = 0, secondaryDelaySeconds = 0;
    double previousSeekPosition = -1, scrubStart = -1;
    double loopStart = -1, loopEnd = -1;
    QString nativePrimary, nativeSecondary;
    SubtitleTimeline subtitleTimeline;
    StudyInteraction studyInteraction;
    StudyServices *studyServices = nullptr;
    bool publishingSubtitle = false, restoringSubtitleTracks = false;
    QHash<QString, QString> embeddedTrackPaths;
    void invalidateSubtitleTools();
    QString currentMediaPath;
    QString rememberedSourceSubtitlePath;
    QString rememberedTranslationSubtitlePath;
    double volumePercent = 60;
    double unmutedVolumePercent = 60;
    double pendingSubtitleSeekSeconds = -1;
    int pendingSubtitleCueIndex = -1;
    bool mediaReady = false;
    QList<QUrl> pendingDroppedSubtitles;
    int currentSecondaryCueIndex = -1;
    PlaybackEngine *playbackEngine;
    int currentPrimaryCueIndex = -1;
    QString statusText = "Ready to explore";
    bool replayActive = false;
    bool replayArmed = false;
    double replayStartSeconds = 0;
    double replayEndSeconds = 0;
};
