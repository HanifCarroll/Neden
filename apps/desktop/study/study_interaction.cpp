#include "study_interaction.h"
#include <QFileInfo>
#include <stdexcept>
StudyTarget StudyInteraction::capture(const QString &text, SubtitleRole role) const {
    const auto &settings = *studyServices->settings();
    StudyTarget target;
    target.positionMs = qRound64(playback.position() * 1000);
    target.selection = text;
    target.role = role;
    target.mediaPath = mediaPath;
    target.sourceLanguage = settings.sourceLanguage();
    target.explanationLanguage = settings.explanationLanguage();
    return timeline.capture(std::move(target), settings.includeSecondary());
}
void StudyInteraction::select(const QString &text, bool secondary) {
    const auto role = secondary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    if (text.trimmed().isEmpty() || (selectedText == text.trimmed() && role == selectedRole))
        return;
    const bool playing = !playback.paused();
    if (selectedText.isEmpty())
        resumeAfterExplanation = wordCardOpen ? resumeAfterWordCard : playing;
    emit started();
    closeWordCard(false);
    if (studyServices)
        studyServices->explanation()->stop();
    selectedText = text.trimmed();
    selectedRole = role;
    lastCommitted.reset();
    emit selectionChanged();
    playback.setPaused(true);
}
void StudyInteraction::requestExplanation(const StudyTarget &target) {
    studyServices->explanation()->explainTarget(target);
    emit explanationRequested();
}
void StudyInteraction::explainSelection() {
    if (!studyServices)
        return;
    try {
        if (selectedText.isEmpty())
            select(timeline.text(qRound64(playback.position() * 1000), selectedRole),
                   selectedRole == SubtitleRole::Secondary);
        if (!selectedText.isEmpty())
            requestExplanation(capture(selectedText, selectedRole));
    } catch (const std::exception &exception) {
        emit error(QString::fromUtf8(exception.what()));
    }
}
void StudyInteraction::commitSelection(bool secondary) {
    selectedRole = secondary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    if (!studyServices || selectedText.isEmpty())
        return;
    try {
        const auto target = capture(selectedText, selectedRole);
        if (lastCommitted && lastCommitted->mediaPath == target.mediaPath &&
            lastCommitted->role == target.role && lastCommitted->startMs == target.startMs &&
            lastCommitted->selection == target.selection)
            return;
        lastCommitted = target;
        requestExplanation(target);
    } catch (const std::exception &exception) {
        emit error(QString::fromUtf8(exception.what()));
    }
}
void StudyInteraction::explainCue(bool secondary) {
    const auto role = secondary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    select(timeline.text(qRound64(playback.position() * 1000), role), secondary);
    explainSelection();
}
void StudyInteraction::explainWord() {
    if (!studyServices || !wordTarget)
        return;
    const auto target = *wordTarget;
    select(target.selection, target.role == SubtitleRole::Secondary);
    requestExplanation(target);
}
bool StudyInteraction::followUp(const QString &question) {
    if (!studyServices || studyServices->explanation()->subject().isEmpty())
        return false;
    const auto &target = studyServices->explanation()->target();
    select(studyServices->explanation()->subject(), target && target->role == SubtitleRole::Secondary);
    const bool sent = studyServices->explanation()->followUp(question);
    if (sent)
        playback.setPaused(true);
    return sent;
}
void StudyInteraction::lookupWordAt(const QString &text, int offset, bool secondary) {
    if (!studyServices ||
        studyServices->settings()->learning().wordProvider == LearningSettings::WordProvider::Off)
        return;
    const auto word = WordLookup::wordAt(text, offset);
    if (word.isEmpty())
        return;
    try {
        auto target = capture(word, secondary ? SubtitleRole::Secondary : SubtitleRole::Primary);
        if (target.sourceCue != text)
            return; // A delayed gesture cannot attach to another cue.
        const bool resume = wordCardOpen ? resumeAfterWordCard : !playback.paused();
        emit started();
        closeWordCard(false);
        resumeAfterWordCard = resume;
        wordCardOpen = true;
        wordTarget = target;
        playback.setPaused(true);
        const auto &settings = *studyServices->settings();
        if (settings.learning().wordProvider == LearningSettings::WordProvider::ContextualAi)
            studyServices->words()->lookupContext(word, target.json(), settings.endpoint(), settings.model(),
                                                  settings.learning().noKeyRequired);
        else
            studyServices->words()->lookup(word, settings.learningSettings());
    } catch (const std::exception &exception) {
        emit error(QString::fromUtf8(exception.what()));
    }
}
void StudyInteraction::closeWordCard(bool resume) {
    if (!wordCardOpen)
        return;
    const bool play = resume && resumeAfterWordCard;
    wordCardOpen = false;
    resumeAfterWordCard = false;
    wordTarget.reset();
    if (studyServices)
        studyServices->words()->cancel();
    if (play)
        playback.setPaused(false);
}
void StudyInteraction::retryWordLookup() {
    if (!studyServices || !wordTarget || studyServices->words()->state() != "failed")
        return;
    const auto &settings = *studyServices->settings();
    studyServices->words()->lookupContext(wordTarget->selection, wordTarget->json(), settings.endpoint(),
                                          settings.model(), settings.learning().noKeyRequired);
}
void StudyInteraction::translateWord() {
    if (!studyServices || !wordTarget)
        return;
    studyServices->words()->lookup(wordTarget->selection, studyServices->settings()->learningSettings(),
                                   true);
}
void StudyInteraction::closeDefinition() {
    if (studyServices)
        studyServices->explanation()->stop();
    if (selectedText.isEmpty())
        return;
    selectedText.clear();
    lastCommitted.reset();
    const bool resume = resumeAfterExplanation;
    resumeAfterExplanation = false;
    if (resume)
        playback.setPaused(false);
    emit selectionChanged();
}
void StudyInteraction::dismiss() {
    closeWordCard();
    closeDefinition();
    reset(true);
}
void StudyInteraction::reset(bool clear) {
    closeWordCard(false);
    if (studyServices) {
        if (clear)
            studyServices->explanation()->invalidateContext();
        else
            studyServices->explanation()->stop();
    }
    selectedText.clear();
    lastCommitted.reset();
    resumeAfterExplanation = false;
    selectedRole = SubtitleRole::Primary;
    emit selectionChanged();
}
void StudyInteraction::saveCard() {
    if (!studyServices)
        return;
    const auto &explanation = *studyServices->explanation();
    // A displayed explanation always saves its original target, even after playback seeks elsewhere.
    if (explanation.target()) {
        const auto &target = *explanation.target();
        studyServices->deck()->saveCard(target.selection, target.sourceCue,
                                        explanation.state() == "complete" ? explanation.answer()
                                                                          : target.translation,
                                        target.positionMs / 1000.0, target.mediaPath);
    } else {
        try {
            const auto target = capture(selectedText, selectedRole);
            studyServices->deck()->saveCard(target.selection, target.sourceCue, target.translation,
                                            target.positionMs / 1000.0, target.mediaPath);
        } catch (const std::exception &exception) {
            emit error(QString::fromUtf8(exception.what()));
        }
    }
}
