#include "explanation_session.h"
#include "network_error.h"
#include <QJsonDocument>
#include <QTimer>
#include <stdexcept>
namespace {
constexpr qsizetype maximumResponse = 8 * 1024 * 1024;
}
ExplanationSession::ExplanationSession(AppSettings &preferences, AiClient::CredentialReader credentialReader,
                                       QObject *parent, QString alternateApiEndpoint)
    : QObject(parent), settings(preferences), aiClient(std::move(credentialReader)),
      alternateEndpoint(std::move(alternateApiEndpoint)) {
    if (!AiClient::allowedEndpoint(QUrl(alternateEndpoint)))
        throw std::invalid_argument("Invalid alternate API endpoint.");
    connect(&settings, &AppSettings::changed, this, &ExplanationSession::stop);
}

void ExplanationSession::explain(const QString &selection, const QString &context) {
    explainContext(QJsonObject{{"selection", selection},
                               {"sourceCue", context},
                               {"sourceLanguage", settings.sourceLanguage()},
                               {"explanationLanguage", settings.explanationLanguage()}});
}

void ExplanationSession::explainTarget(const StudyTarget &target) {
    begin(target, target.json());
}
void ExplanationSession::explainContext(const QJsonObject &context) {
    begin(StudyTarget::fromJson(context), context);
}
void ExplanationSession::begin(const StudyTarget &target, const QJsonObject &context) {
    stop();
    try {
        conversation.begin(context);
        activeTarget = target;
        subjectText = target.selection;
        subjectContext = target.sourceCue;
        launchConversation();
    } catch (const std::exception &error) {
        requestState = "failed";
        setMessage(QString::fromUtf8(error.what()));
    }
}

bool ExplanationSession::followUp(const QString &question) {
    try {
        conversation.followUp(question);
        launchConversation();
        return true;
    } catch (const std::exception &error) {
        setMessage(QString::fromUtf8(error.what()));
        return false;
    }
}

void ExplanationSession::launchConversation(bool fallback) {

    // 1. Build the entire frozen context and completed conversation; never truncate it.
    answerText.clear();
    requestState = "loading";
    if (!fallback)
        emit replyStarted();
    const quint64 requestGenerationAtStart = ++requestGeneration;
    fallbackWaiting = !fallback && settings.learningJson().value("fallbackEnabled").toBool();
    if (fallbackWaiting) {
        QTimer::singleShot(5000, this, [this, requestGenerationAtStart] {
            if (requestGenerationAtStart != requestGeneration || !fallbackWaiting ||
                !answerText.trimmed().isEmpty())
                return;

            // Only an explicitly enabled fallback can replace an unanswered attempt.
            if (aiReply) {
                auto *previous = aiReply.data();
                aiReply = nullptr;
                previous->disconnect(this);
                previous->abort();
                previous->deleteLater();
            }
            launchConversation(true);
        });
    }
    try {
        const bool useLunaFirst = settings.learningJson().value("primaryProvider").toString() == "luna";
        const bool luna = fallback ? !useLunaFirst : useLunaFirst;
        const QString endpoint = luna ? alternateEndpoint : settings.endpoint();
        const QString model = luna ? "gpt-6-luna" : settings.model();
        ChatRequest request;
        request.endpoint = endpoint;
        request.model = model;
        request.messages = conversation.messages();
        request.noKeyRequired = !luna && settings.learning().noKeyRequired;
        conversation.setProvider(model, fallback);

        // 2. Bind chunks, completion and cancellation to this exact network reply.
        pendingEvents.clear();
        sawDone = false;
        truncated = false;
        requestState = "loading";
        statusText = "Requesting explanation…";
        auto *reply = aiClient.post(request);
        aiReply = reply;
        connect(reply, &QNetworkReply::readyRead, this, &ExplanationSession::consumeEvents);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            if (aiReply != reply) {
                reply->deleteLater();
                return;
            }

            consumeEvents();
            if (aiReply != reply)
                return;
            const auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError || code != 200) {
                requestState = answerText.isEmpty() ? "failed" : "incomplete";
                statusText = networkFailureMessage(code, reply->error(), "Explanation failed");
            } else if (!sawDone || truncated || answerText.isEmpty()) {
                requestState = "incomplete";
                statusText = "The response ended before a complete answer arrived.";
            } else {
                requestState = "complete";
                statusText = "Explanation complete. Review it before saving a flashcard.";
            }
            if (fallbackWaiting && answerText.trimmed().isEmpty() && requestState != "complete") {
                requestState = "loading";
                statusText =
                    "The primary attempt ended without an answer. Waiting for the enabled 5-second fallback…";
            } else {
                fallbackWaiting = false;
                conversation.finish(requestState, requestState == "complete" ? QString() : statusText);
            }
            aiReply = nullptr;
            reply->deleteLater();
            emit changed();
        });
        QTimer::singleShot(120000, reply, [this, reply] {
            if (aiReply == reply) {
                stop();
                setMessage("Request timed out; any partial answer is incomplete.");
            }
        });
        emit changed();
    } catch (const std::exception &error) {
        if (fallbackWaiting) {
            requestState = "loading";
            setMessage("The primary connection is unavailable. Waiting for the enabled 5-second fallback…");
        } else {
            requestState = "failed";
            conversation.finish("failed");
            setMessage(QString::fromUtf8(error.what()));
        }
    }
}

void ExplanationSession::consumeEvents() {
    if (!aiReply)
        return;

    // 1. Buffer complete SSE lines; UTF-8 sequences may cross network chunks.
    pendingEvents += aiReply->readAll();
    if (pendingEvents.size() + answerText.size() * 4 > maximumResponse) {
        stop();
        setMessage("The response exceeded the size limit; the answer is incomplete.");
        return;
    }

    // 2. Accept only the protocol's text deltas and explicit end marker.
    while (pendingEvents.contains('\n')) {
        const auto end = pendingEvents.indexOf('\n');
        const auto line = pendingEvents.left(end).trimmed();
        pendingEvents.remove(0, end + 1);
        if (!line.startsWith("data:"))
            continue;
        const auto data = line.mid(5).trimmed();
        if (data == "[DONE]") {
            sawDone = true;
            continue;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject() ||
            document.object().contains("error")) {
            stop();
            setMessage("The provider returned an invalid stream; any partial answer is incomplete.");
            return;
        }
        const auto choices = document.object().value("choices").toArray();
        if (choices.isEmpty())
            continue;
        const auto reason = choices.first().toObject().value("finish_reason").toString();
        if (reason == "length" || reason == "content_filter")
            truncated = true;
        const auto text = choices.first().toObject().value("delta").toObject().value("content").toString();
        try {
            conversation.append(text);
        } catch (const std::exception &error) {
            stop();
            setMessage(QString::fromUtf8(error.what()));
            return;
        }
        answerText += text;
        if (!answerText.trimmed().isEmpty())
            fallbackWaiting = false;
        requestState = "streaming";
        emit changed();
    }
}

void ExplanationSession::stop() {
    if (!aiReply && !fallbackWaiting)
        return;

    ++requestGeneration;
    fallbackWaiting = false;
    if (aiReply) {
        auto *reply = aiReply.data();
        aiReply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    pendingEvents.clear();
    requestState = answerText.isEmpty() ? "failed" : "incomplete";
    conversation.finish(requestState, "Stopped");
    statusText = "Stopped. The partial answer has not been marked complete.";
    emit changed();
}

void ExplanationSession::retryExplanation() {
    if (requestState != "failed" && requestState != "incomplete")
        return;

    try {
        conversation.retry();
        launchConversation();
    } catch (const std::exception &error) {
        setMessage(QString::fromUtf8(error.what()));
    }
}

void ExplanationSession::invalidateContext() {
    stop();
    answerText.clear();
    subjectText.clear();
    subjectContext.clear();
    conversation.clear();
    activeTarget.reset();
    requestState = "idle";
    emit changed();
}
