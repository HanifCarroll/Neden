#include "explanation_stream.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QStringList ExplanationStream::append(const QByteArray &bytes) {
    QStringList deltas;
    if (!failure.isEmpty())
        return deltas;
    constexpr qsizetype maximumResponse = 8 * 1024 * 1024;
    if (bytes.size() > maximumResponse - pending.size() - answerCharacters * 4) {
        failure = "The response exceeded the size limit; the answer is incomplete.";
        pending.clear();
        return deltas;
    }
    pending += bytes;
    while (pending.contains('\n')) {
        const auto end = pending.indexOf('\n');
        const auto line = pending.left(end).trimmed();
        pending.remove(0, end + 1);
        if (!line.startsWith("data:"))
            continue;
        const auto data = line.mid(5).trimmed();
        if (data == "[DONE]" && !sawDone) {
            sawDone = true;
            continue;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(data, &parseError);
        if (sawDone || parseError.error != QJsonParseError::NoError || !document.isObject() ||
            document.object().contains("error")) {
            failure = "The provider returned an invalid stream; any partial answer is incomplete.";
            pending.clear();
            break;
        }
        const auto choices = document.object().value("choices").toArray();
        if (choices.isEmpty())
            continue;
        const auto choice = choices.first().toObject();
        const auto reason = choice.value("finish_reason").toString();
        truncated = truncated || reason == "length" || reason == "content_filter";
        const auto text = choice.value("delta").toObject().value("content").toString();
        answerCharacters += text.size();
        if (answerCharacters * 4 > maximumResponse) {
            failure = "The response exceeded the size limit; the answer is incomplete.";
            pending.clear();
            break;
        }
        deltas.append(text);
    }
    return deltas;
}
