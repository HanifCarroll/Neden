#pragma once
#include "cues.h"
#include "study_tools.h"
#include "recent_videos.h"
#include <QTimer>
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
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaChanged)
    Q_PROPERTY(QString sourceSubtitleName READ sourceSubtitleName NOTIFY tracksChanged)
    Q_PROPERTY(QString translationSubtitleName READ translationSubtitleName NOTIFY tracksChanged)
    Q_PROPERTY(bool replaying READ replaying NOTIFY replayingChanged)
    Q_PROPERTY(int activeCue READ activeCue NOTIFY cueChanged)
    Q_PROPERTY(bool canPreviousSubtitle READ canPreviousSubtitle NOTIFY cueChanged)
    Q_PROPERTY(bool canNextSubtitle READ canNextSubtitle NOTIFY cueChanged)
public:
    Q_PROPERTY(StudyTools *study READ study CONSTANT)
    StudyTools *study() const { return studyTools; }
    void setStudyTools(StudyTools *tools);
    void setRecentVideos(RecentVideos *videos);
    ~LearningSession() override;
    bool hasMedia() const { return !currentMediaPath.isEmpty(); }
    QString mediaTitle() const;
    QString sourceSubtitleName() const;
    QString translationSubtitleName() const;
    Q_INVOKABLE void checkpoint();
    explicit LearningSession(PlaybackEngine *engine, QObject *parent = nullptr);
    Q_PROPERTY(CueModel *cueModel READ cueModel CONSTANT)
    CueModel *cueModel() { return &model; }
    void replaceCues(QVector<Cue> next);
    void setPaused(bool value);
    CueModel model;
    double position() const { return engine->position(); }
    double duration() const { return engine->duration(); }
    bool paused() const { return engine->paused(); }
    QString subtitle() const;
    QString translation() const;
    int translationCueCount() const { return translationCues.size(); }
    QString selection() const { return selectedText; }
    QString status() const { return statusText; }
    bool replaying() const { return replayActive; }
    int activeCue() const { return currentCue; }
    Q_INVOKABLE void openVideo(const QUrl &url);
    Q_INVOKABLE void openTranslation(const QUrl &url);
    Q_INVOKABLE void explainSelection();
    Q_INVOKABLE void openSubtitles(const QUrl &url);
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void rate(double speed);
    double volumeLevel() const { return audioLevel; }
    Q_INVOKABLE void volume(double level);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void select(const QString &text);
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
private:
    int previousSubtitleIndex() const;
    int nextSubtitleIndex() const;
    void playSubtitle(int index);
    void finishReplay();
    void syncPosition();
    void resetInteraction();
    void finishOpening(const QString &path);
    StudyTools *studyTools = nullptr;
    RecentVideos *recentVideos = nullptr;
    QTimer checkpointTimer;
    QString currentMediaPath;
    QString sourceSubtitlePath;
    QString translationSubtitlePath;
    double audioLevel = 60;
    double unmutedLevel = 60;
    double restorePosition = -1;
    double subtitleSeekTarget = -1;
    int subtitleSeekIndex = -1;
    bool mediaReady = false;
    bool rememberCurrent = true;
    QVector<Cue> translationCues;
    int translationCue = -1;
    PlaybackEngine *engine;
    int currentCue = -1;
    QString selectedText;
    QString statusText = "Ready to explore";
    bool resumeAfterDefinition = false;
    bool replayActive = false;
    bool replayArmed = false;
    double replayStart = 0;
    double replayEnd = 0;
};
