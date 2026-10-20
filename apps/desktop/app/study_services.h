#pragma once
#include "profile_store.h"
#include "app_settings.h"
#include "flashcard_deck.h"
#include "explanation_session.h"
#include "subtitle_service.h"
#include "word_lookup.h"
class StudyServices : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Study modules are supplied by the application")
    Q_PROPERTY(AppSettings *settings READ settings CONSTANT)
    Q_PROPERTY(ExplanationSession *explanation READ explanation CONSTANT)
    Q_PROPERTY(SubtitleService *subtitles READ subtitles CONSTANT)
    Q_PROPERTY(FlashcardDeck *deck READ deck CONSTANT)
    Q_PROPERTY(WordLookup *words READ words CONSTANT)
public:
    using CredentialReader = AiClient::CredentialReader;
    StudyServices(QString directory, CredentialReader reader, QObject *parent = nullptr,
                  QString subtitleApi = "https://api.opensubtitles.com/api/v1",
                  QString alternateApi = "https://api.openai.com/v1")
        : QObject(parent), profile(directory), preferences(profile, this), flashcards(profile, this),
          explanations(preferences, reader, this, alternateApi),
          subtitleAcquisition(directory, preferences, reader, this, subtitleApi), wordLookup(reader, this) {
        connect(&preferences, &AppSettings::changed, &wordLookup, &WordLookup::cancel);
    }
    AppSettings *settings() { return &preferences; }
    const AppSettings *settings() const { return &preferences; }
    ExplanationSession *explanation() { return &explanations; }
    SubtitleService *subtitles() { return &subtitleAcquisition; }
    FlashcardDeck *deck() { return &flashcards; }
    WordLookup *words() { return &wordLookup; }

private:
    ProfileStore profile;
    AppSettings preferences;
    FlashcardDeck flashcards;
    ExplanationSession explanations;
    SubtitleService subtitleAcquisition;
    WordLookup wordLookup;
};
