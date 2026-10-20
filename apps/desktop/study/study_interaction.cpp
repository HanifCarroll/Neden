#include "study_interaction.h"
#include <QFileInfo>
#include <stdexcept>
StudyTarget StudyInteraction::capture(const QString &text, SubtitleRole role) const {
    const auto &settings = *services->settings();
    StudyTarget target;
    target.positionMs = qRound64(playback.position() * 1000);
    target.selection = text;
    target.role = role;
    target.mediaPath = media;
    target.sourceLanguage = settings.sourceLanguage();
    target.explanationLanguage = settings.explanationLanguage();
    return timeline.capture(std::move(target), settings.includeSecondary());
}
void StudyInteraction::select(const QString &text, bool secondary) {
    const auto role = secondary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    if (text.trimmed().isEmpty() || (selected == text.trimmed() && role == selectedRole))
        return;
    const bool playing = !playback.paused();
    if (selected.isEmpty())
        resumeDefinition = wordActive ? resumeWord : playing;
    emit started();
    closeWordCard(false);
    if (services)
        services->explanation()->stop();
    selected = text.trimmed();
    selectedRole = role;
    lastCommitted.reset();
    emit selectionChanged();
    playback.setPaused(true);
}
void StudyInteraction::request(const StudyTarget &target) {
    services->explanation()->explainTarget(target);
    emit explanationRequested();
}
void StudyInteraction::explainSelection() {
    if (!services)
        return;
    try {
        if (selected.isEmpty())
            select(timeline.text(qRound64(playback.position() * 1000), selectedRole),
                   selectedRole == SubtitleRole::Secondary);
        if (!selected.isEmpty())
            request(capture(selected, selectedRole));
    } catch (const std::exception &e) {
        emit error(QString::fromUtf8(e.what()));
    }
}
void StudyInteraction::commitSelection(bool secondary) {
    selectedRole = secondary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    if (!services || selected.isEmpty())
        return;
    try {
        const auto target = capture(selected, selectedRole);
        if (lastCommitted && lastCommitted->mediaPath == target.mediaPath &&
            lastCommitted->role == target.role && lastCommitted->startMs == target.startMs &&
            lastCommitted->selection == target.selection)
            return;
        lastCommitted = target;
        request(target);
    } catch (const std::exception &e) {
        emit error(QString::fromUtf8(e.what()));
    }
}
void StudyInteraction::explainCue(bool secondary) {
    const auto role = secondary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    select(timeline.text(qRound64(playback.position() * 1000), role), secondary);
    explainSelection();
}
void StudyInteraction::explainWord() {
    if (!services || !wordTarget)
        return;
    const auto target = *wordTarget;
    select(target.selection, target.role == SubtitleRole::Secondary);
    request(target);
}
bool StudyInteraction::followUp(const QString &question) {
    if (!services || services->explanation()->subject().isEmpty())
        return false;
    const auto &target = services->explanation()->target();
    select(services->explanation()->subject(), target && target->role == SubtitleRole::Secondary);
    const bool sent = services->explanation()->followUp(question);
    if (sent)
        playback.setPaused(true);
    return sent;
}
void StudyInteraction::lookupWordAt(const QString &text, int offset, bool secondary) {
    if (!services || services->settings()->learning().wordProvider == LearningSettings::Off)
        return;
    const auto word = WordLookup::wordAt(text, offset);
    if (word.isEmpty())
        return;
    try {
        auto target = capture(word, secondary ? SubtitleRole::Secondary : SubtitleRole::Primary);
        if (target.sourceCue != text)
            return; // A delayed gesture cannot attach to another cue.
        const bool resume = wordActive ? resumeWord : !playback.paused();
        emit started();
        closeWordCard(false);
        resumeWord = resume;
        wordActive = true;
        wordTarget = target;
        playback.setPaused(true);
        const auto &settings = *services->settings();
        if (settings.learning().wordProvider == LearningSettings::ContextualAi)
            services->words()->lookupContext(word, target.json(), settings.endpoint(), settings.model(),
                                             settings.learning().noKeyRequired);
        else
            services->words()->lookup(word, settings.learningSettings());
    } catch (const std::exception &e) {
        emit error(QString::fromUtf8(e.what()));
    }
}
void StudyInteraction::closeWordCard(bool resume) {
    if (!wordActive)
        return;
    const bool play = resume && resumeWord;
    wordActive = false;
    resumeWord = false;
    wordTarget.reset();
    if (services)
        services->words()->cancel();
    if (play)
        playback.setPaused(false);
}
void StudyInteraction::retryWordLookup() {
    if (!services || !wordTarget || services->words()->state() != "failed")
        return;
    const auto &settings = *services->settings();
    services->words()->lookupContext(wordTarget->selection, wordTarget->json(), settings.endpoint(),
                                     settings.model(), settings.learning().noKeyRequired);
}
void StudyInteraction::translateWord() {
    if (!services || !wordTarget)
        return;
    services->words()->lookup(wordTarget->selection, services->settings()->learningSettings(), true);
}
void StudyInteraction::closeDefinition() {
    if (services)
        services->explanation()->stop();
    if (selected.isEmpty())
        return;
    selected.clear();
    lastCommitted.reset();
    const bool resume = resumeDefinition;
    resumeDefinition = false;
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
    if (services) {
        if (clear)
            services->explanation()->invalidateContext();
        else
            services->explanation()->stop();
    }
    selected.clear();
    lastCommitted.reset();
    resumeDefinition = false;
    selectedRole = SubtitleRole::Primary;
    emit selectionChanged();
}
void StudyInteraction::saveCard() {
    if (!services)
        return;
    const auto &explanation = *services->explanation();
    // A displayed explanation always saves its original target, even after playback seeks elsewhere.
    if (explanation.target()) {
        const auto &target = *explanation.target();
        services->deck()->saveCard(target.selection, target.sourceCue,
                                   explanation.state() == "complete" ? explanation.answer()
                                                                     : target.translation,
                                   target.positionMs / 1000.0, target.mediaPath);
    } else {
        try {
            const auto target = capture(selected, selectedRole);
            services->deck()->saveCard(target.selection, target.sourceCue, target.translation,
                                       target.positionMs / 1000.0, target.mediaPath);
        } catch (const std::exception &e) {
            emit error(QString::fromUtf8(e.what()));
        }
    }
}
