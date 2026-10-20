#pragma once
#include "study_services.h"
#include "subtitle_timeline.h"
#include "playback_engine.h"

// Owns the entire study gesture lifecycle, including the pause it may temporarily own.
class StudyInteraction : public QObject {
    Q_OBJECT
public:
    StudyInteraction(PlaybackEngine &playback, SubtitleTimeline &timeline, QObject *parent = nullptr)
        : QObject(parent), playback(playback), timeline(timeline) {}
    void setServices(StudyServices *value) { services = value; }
    void setMedia(const QString &value) { media = value; }
    QString selection() const { return selected; }
    void select(const QString &text, bool secondary = false);
    void commitSelection(bool secondary = false);
    void explainSelection();
    void explainCue(bool secondary = false);
    void explainWord();
    bool followUp(const QString &question);
    void lookupWordAt(const QString &text, int offset, bool secondary = false);
    void closeWordCard(bool resume = true);
    void retryWordLookup();
    void translateWord();
    void closeDefinition();
    void reset(bool clearConversation = false);
    void dismiss();
    void cueChanged() { closeWordCard(false); }
    void saveCard();

private:
    StudyTarget capture(const QString &text, SubtitleRole role) const;
    void request(const StudyTarget &target);
    PlaybackEngine &playback;
    SubtitleTimeline &timeline;
    StudyServices *services = nullptr;
    QString selected, media;
    std::optional<StudyTarget> lastCommitted;
    SubtitleRole selectedRole = SubtitleRole::Primary;
    bool resumeDefinition = false, wordActive = false, resumeWord = false;
    std::optional<StudyTarget> wordTarget;
signals:
    void selectionChanged();
    void started();
    void explanationRequested();
    void error(QString message);
};
