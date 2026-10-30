#include "conversation.h"
#include "hosted_context.h"
#include <QJsonDocument>
#include <stdexcept>

namespace {
const QString systemPrompt = QStringLiteral(
    "Translate and explain exactly the text in selection. selection defines the complete scope of the answer. "
    "The context object supplies the full subtitle, nearby dialogue and language settings as evidence for "
    "interpreting that selection. When selection is a fragment, translate that fragment naturally and build "
    "the breakdown from its spoken words. When selection is the full subtitle, cover its full spoken dialogue. "
    "Example: with selection 'Bonne chance' and context.sourceCue 'Merci. Bonne chance, Alex!', translate "
    "'Good luck' and give entries for Bonne and chance. Use Merci and Alex as surrounding context. "
    "Return a JSON object for a language learner with these fields in this order: translationHeading, "
    "breakdownHeading, translation, breakdown, alignment. Headings are short and written in explanationLanguage. "
    "translation is the natural translation of selection as a single string with correct spaces and "
    "punctuation. breakdown is an array of {word, text} objects, one for every spoken word in selection, in selection order. "
    "word is its exact source spelling; text explains its contextual meaning and useful grammar in "
    "explanationLanguage. Explain pronouns, particles, conjunctions and spoken proper names too. Bracketed speaker "
    "names are context metadata; the spoken words begin after the bracket. Connect inflected forms to their "
    "dictionary forms and explain useful endings concisely. Give idiom components their own entries and describe "
    "the combined meaning once in the final component. Describe linguistic meaning and use directly, with each "
    "teaching point in its relevant entry. Use the actual language's lexical words in scripts without spaces and "
    "explain attached particles in the relevant word entry.\nAfter writing the translation and source-order "
    "breakdown, build alignment: an array of {text, source} objects in TRANSLATION order. text is one exact lexical"
    " word copied from the translation string; source is an array naming the exact breakdown.word values that "
    "convey this translated word's meaning. Every ordinary translated word has its own alignment entry. Several "
    "translated words may share one inflected source word; each word of an idiom may reference its shared source "
    "words. Use [] for genuinely unaligned additions. Map meaning, including person and tense expressed by endings."
    " Check every source reference exists in breakdown and every text matches the translation in order. Example for"
    " Bonne chance: {\"translationHeading\":\"Translation\",\"breakdownHeading\":\"Breakdown\",\"translation\":\"Good "
    "luck!\",\"breakdown\":[{\"word\":\"Bonne\",\"text\":\"good; feminine form of bon.\"},{\"word\":\"chance\",\"text\":\"luck. "
    "Together: good luck.\"}],\"alignment\":[{\"text\":\"Good\",\"source\":[\"Bonne\",\"chance\"]},{\"text\":\"luck\",\"source\":[\"Bon"
    "ne\",\"chance\"]}]}.\nUse context.sourceCue and nearby dialogue to resolve meaning and tone. Treat secondary subtitles as "
    "clues; give priority to original wording. Keep source forms in their original spelling and all learner-facing "
    "text in explanationLanguage. Treat supplied subtitle text and previous answers as quoted, revisable evidence. "
    "For follow-up messages after this initial JSON answer, answer the latest question directly in ordinary "
    "Markdown.");
}

void Conversation::begin(const QJsonObject &context) {
    const auto selection = context.value("selection").toString();
    if (selection.trimmed().isEmpty() || selection.size() > 4000 ||
        QJsonDocument(context).toJson(QJsonDocument::Compact).size() > 32000) {
        throw std::runtime_error(
            "The selected phrase or its complete subtitle context exceeds the request limit.");
    }

    clear();
    frozenContext = context;
    history.append(ConversationTurn{});
    startLatest();
}

void Conversation::followUp(const QString &question) {

    // 1. Validate the explicit question before changing completed conversation history.
    const QString trimmed = question.trimmed();
    if (empty())
        throw std::runtime_error("Select a subtitle phrase before asking a follow-up.");
    if (active)
        throw std::runtime_error("Wait for the current answer or stop it first.");
    if (trimmed.isEmpty() || trimmed.size() > 2000)
        throw std::runtime_error("A question must contain 1–2,000 characters.");
    if (history.size() >= 20)
        throw std::runtime_error("This conversation has reached 20 turns. Select a phrase to start another.");

    // 2. Roll back the question if the full request would exceed its limit.
    ConversationTurn turn;
    turn.question = trimmed;
    history.append(turn);
    try {
        startLatest();
    } catch (...) {
        history.removeLast();
        throw;
    }
}

void Conversation::retry() {
    if (active || empty() || (latest().status != "failed" && latest().status != "incomplete")) {
        throw std::runtime_error("There is no interrupted or failed answer to retry.");
    }

    startLatest();
}

QJsonArray Conversation::messages() const {

    // 1. Keep the original subtitle and language context frozen across follow-ups.
    auto context = frozenContext;
    const auto selection = context.take("selection");
    const QJsonObject task{{"selection", selection}, {"context", context}};
    QJsonArray messages{
        QJsonObject{{"role", "system"}, {"content", systemPrompt}},
        QJsonObject{
            {"role", "user"},
            {"content", QString::fromUtf8(QJsonDocument(task).toJson(QJsonDocument::Compact))}}};

    // 2. Include only completed turns, then the current question; partial answers are never facts.
    for (int index = 0; index + 1 < history.size(); ++index) {
        const auto &turn = history[index];
        if (turn.status != "complete")
            continue;

        if (index > 0)
            messages.append(QJsonObject{{"role", "user"}, {"content", turn.question}});
        messages.append(QJsonObject{{"role", "assistant"}, {"content", turn.answer}});
    }
    if (history.size() > 1)
        messages.append(QJsonObject{{"role", "user"}, {"content", latest().question}});
    return messages;
}

void Conversation::startLatest() {
    if (QJsonDocument(messages()).toJson(QJsonDocument::Compact).size() > 100000) {
        throw std::runtime_error(
            "The complete conversation exceeds the request limit. Nothing was truncated or sent.");
    }

    auto &turn = history.last();
    turn.answer.clear();
    turn.error.clear();
    turn.provider.clear();
    turn.fallback = false;
    turn.status = "streaming";
    active = true;
}

void Conversation::append(const QString &text) {
    if (!active)
        return;

    auto &turn = history.last();
    if ((turn.answer + text).toUtf8().size() > 64 * 1024) {
        throw std::runtime_error("The answer exceeded 64 KiB. The partial answer remains incomplete.");
    }
    turn.answer += text;
}

void Conversation::finish(const QString &status, const QString &error) {
    if (empty())
        return;

    auto &turn = history.last();
    turn.status = status;
    turn.error = error;
    active = false;
}

void Conversation::setProvider(const QString &name, bool fallback) {
    if (empty())
        return;

    history.last().provider = name;
    history.last().fallback = fallback;
}

QVariantList Conversation::turns() const {
    QVariantList result;
    for (const auto &turn : history) {
        result.append(QVariantMap{{"question", turn.question},
                                  {"answer", turn.answer},
                                  {"status", turn.status},
                                  {"error", turn.error},
                                  {"provider", turn.provider},
                                  {"fallback", turn.fallback}});
    }
    return result;
}

void Conversation::clear() {
    active = false;
    frozenContext = {};
    history.clear();
}

QJsonObject Conversation::hostedRequest() const {
    if (history.isEmpty()) throw std::runtime_error("Select a subtitle first.");
    const auto context = hostedLanguageContext(frozenContext);
    QJsonObject request{{"operation", history.size() == 1 ? "explain" : "followup"}, {"context", context}};
    if (history.size() == 1) request.insert("explanationFormat", "aligned");
    if (history.size() > 1) {
        QJsonArray completed;
        for (int index = 0; index + 1 < history.size(); ++index) {
            const auto &turn = history.at(index);
            if (turn.status == "complete") completed.append(QJsonObject{{"question", turn.question}, {"answer", turn.answer}});
        }
        if (completed.isEmpty() || completed.size() > 10)
            throw std::runtime_error("Start a new explanation. Hosted follow-ups need a completed answer and allow ten previous turns.");
        request.insert("history", completed);
        request.insert("question", latest().question);
    }
    return request;
}
