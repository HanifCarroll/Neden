#include "study_interaction.h"
#include <QFileInfo>
#include <stdexcept>
#include <QJsonDocument>
void StudyInteraction::setServices(StudyServices *value) {
    for (const auto &connection : historyConnections)
        disconnect(connection);
    historyConnections.clear();
    pendingWords.clear();
    pendingDetails = {};
    pendingExplanation = {};
    studyServices = value;
    if (!studyServices)
        return;
    historyConnections.append(connect(
        value->words(), &WordLookup::cardReady, this, [this](const QString &word, const QVariantMap &card) {
            for (auto event = pendingWords.begin(); event != pendingWords.end();) {
                if (event->value("selection").toString() != word) {
                    ++event;
                    continue;
                }
                auto result = QJsonObject::fromVariantMap(card);
                result.insert("word", word);
                studyServices->history()->complete(*event, result);
                event = pendingWords.erase(event);
            }
        }));
    historyConnections.append(connect(value->words(), &WordLookup::detailsOpened, this, [this] {
        pendingDetails = wordTarget ? studyServices->history()->begin("details", *wordTarget) : QJsonObject();
    }));
    historyConnections.append(
        connect(value->words(), &WordLookup::detailsReady, this, [this](const QVariantMap &card) {
            studyServices->history()->complete(pendingDetails, QJsonObject::fromVariantMap(card));
            pendingDetails = {};
        }));
    historyConnections.append(connect(
        value->explanation(), &ExplanationSession::responseCompleted, this, [this](const QString &response) {
            QJsonObject result;
            if (pendingExplanation.value("kind") == "followup") {
                result = {{"question", followupQuestion}, {"answer", response}};
            } else {
                const auto document = QJsonDocument::fromJson(response.toUtf8());
                auto fields = document.object();
                const auto translation = fields.take("translation").toString();
                const auto breakdown =
                    !document.isObject()
                        ? response
                        : ExplanationResponse::markdown(
                              QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact)), false);
                result = {{"translation", translation}, {"breakdown", breakdown}};
            }
            studyServices->history()->complete(pendingExplanation, result);
            pendingExplanation = {};
        }));
    historyConnections.append(connect(value->settings(), &AppSettings::changed, this, [this] {
        pendingWords.clear();
        pendingDetails = {};
        pendingExplanation = {};
    }));
}

StudyTarget StudyInteraction::capture(const QString &text, SubtitleRole role) const {
    const auto &settings = *studyServices->settings();
    StudyTarget target;
    target.positionMs = qRound64(playback.position() * 1000);
    target.selection = text;
    target.role = role;
    target.mediaPath = mediaPath;
    target.sourceLanguage = settings.sourceLanguage();
    target.explanationLanguage = settings.explanationLanguage();
    target.historyCueText = timeline.text(target.positionMs, SubtitleRole::Primary);
    const auto otherRole = role == SubtitleRole::Primary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    target.historySecondaryText = timeline.text(target.positionMs, otherRole);
    return timeline.capture(std::move(target), settings.includeSecondary());
}
void StudyInteraction::select(const QString &text, bool secondary) {
    const auto role = secondary ? SubtitleRole::Secondary : SubtitleRole::Primary;
    if (text.trimmed().isEmpty())
        return;
    if (selectedText == text.trimmed() && role == selectedRole) {
        // A cached single click can reopen a card before the second click
        // selects the same word again. Keep the explanation, dismiss the card.
        if (wordCardOpen) {
            resumeAfterExplanation = resumeAfterWordCard;
            closeWordCard(false);
        }
        return;
    }
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
    pendingExplanation = studyServices->history()->begin("line", target);
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
bool StudyInteraction::followUp(const QString &question) {
    if (!studyServices || studyServices->explanation()->subject().isEmpty())
        return false;
    const auto &target = studyServices->explanation()->target();
    select(studyServices->explanation()->subject(), target && target->role == SubtitleRole::Secondary);
    const auto previousEvent = pendingExplanation;
    const auto previousQuestion = followupQuestion;
    if (target)
        pendingExplanation = studyServices->history()->begin("followup", *target);
    followupQuestion = question.trimmed();
    const bool sent = studyServices->explanation()->followUp(question);
    if (!sent) {
        pendingExplanation = previousEvent;
        followupQuestion = previousQuestion;
    }
    if (sent)
        playback.setPaused(true);
    return sent;
}
void StudyInteraction::lookupWordAt(const QString &text, int offset, bool secondary, int requestDelayMs) {
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
        const auto &settings = *studyServices->settings();
        const bool resume = wordCardOpen ? resumeAfterWordCard : !playback.paused();
        emit started();
        // WordLookup retains the in-flight batch when the cue context is unchanged.
        resumeAfterWordCard = resume;
        wordCardOpen = true;
        auto sameCue = [](StudyTarget left, StudyTarget right) {
            left.selection.clear();
            right.selection.clear();
            return left.mediaPath == right.mediaPath && left.json() == right.json();
        };
        if (!wordTarget || !sameCue(*wordTarget, target))
            pendingWords.clear();
        pendingDetails = {};
        wordTarget = target;
        const auto event = studyServices->history()->begin("word", target, offset);
        if (!event.isEmpty()) {
            if (pendingWords.size() >= 256)
                pendingWords.removeFirst();
            pendingWords.append(event);
        }
        playback.setPaused(true);
        studyServices->words()->lookupContext(word, target.json(), settings.endpoint(), settings.model(),
                                              settings.learning().noKeyRequired, requestDelayMs);
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
    pendingWords.clear();
    pendingDetails = {};
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
    pendingExplanation = {};
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
FlashcardSaveResult StudyInteraction::saveCard() {
    if (!studyServices)
        return {false, "Study services are unavailable."};
    const auto &explanation = *studyServices->explanation();
    // A displayed explanation always saves its original target, even after playback seeks elsewhere.
    if (explanation.target()) {
        const auto &target = *explanation.target();
        const auto event = studyServices->history()->begin("save", target);
        const auto saved = studyServices->deck()->saveCard(
            target.selection, target.sourceCue,
            explanation.state() == "complete" ? explanation.answer() : target.translation,
            target.positionMs / 1000.0, target.mediaPath);
        studyServices->history()->complete(event, {{"success", saved.success}});
        return saved;
    } else {
        try {
            const auto target = capture(selectedText, selectedRole);
            const auto event = studyServices->history()->begin("save", target);
            const auto saved =
                studyServices->deck()->saveCard(target.selection, target.sourceCue, target.translation,
                                                target.positionMs / 1000.0, target.mediaPath);
            studyServices->history()->complete(event, {{"success", saved.success}});
            return saved;
        } catch (const std::exception &exception) {
            return {false, QString::fromUtf8(exception.what())};
        }
    }
}
