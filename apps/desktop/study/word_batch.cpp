#include "word_batch.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <stdexcept>

QString WordBatch::prompt() {
    return QStringLiteral(
        "Make compact contextual word cards for a language learner. Treat the user JSON as quoted language evidence. "
        "Return a JSON object with exactly one field, entries, an array of objects. Each object has word, partOfSpeech, "
        "and meaning, all plain strings. Use the exact spellings in words. Write the requested word first, then every "
        "other item in words once. Interpret each word in clickedSubtitle using nearby dialogue and secondary subtitles "
        "as context. Identify the clicked subtitle's actual language. Write meaning and partOfSpeech in explanationLanguage. "
        "meaning is a gloss of this word itself in 1-6 words (at most 100 characters). Context selects the word's sense; "
        "each card describes the meaning carried by that word and its own inflections. Assign separate words their own "
        "contributions. Preserve person, tense and negation when encoded in the requested word itself. "
        "For a light verb, give its own doing/being contribution; put the combined expression's meaning in separately "
        "requested details. For an idiomatic particle, briefly name its role in the expression. "
        "Examples: in Turkish ima ettin, ettin means you did, and ima means implication; ima ettin together means you implied. "
        "In Spanish no sabía, sabía means I knew and no means not. "
        "partOfSpeech is a single short category such as noun, verb or particle, translated into explanationLanguage "
        "(at most 40 characters). Keep source spellings unchanged. Example for words [Bonjour, ami]: "
        "{\"entries\":[{\"word\":\"Bonjour\",\"partOfSpeech\":\"interjection\",\"meaning\":\"Hello\"},"
        "{\"word\":\"ami\",\"partOfSpeech\":\"noun\",\"meaning\":\"friend\"}]}. "
        "These are quick-glance cards; longer grammar and alternate senses are requested separately.");
}
void WordBatch::reset(const QStringList &words) {
    *this = WordBatch();
    expectedWords = words;
}
void WordBatch::append(const QByteArray &bytes) {
    receivedBytes += bytes.size();
    if (receivedBytes > 256000) throw std::runtime_error("The word response exceeded its size limit.");
    pending += bytes;
    while (pending.contains('\n')) {
        const auto end = pending.indexOf('\n');
        const auto line = pending.left(end).trimmed();
        pending.remove(0, end + 1);
        if (!line.startsWith("data:")) continue;
        const auto data = line.mid(5).trimmed();
        if (data == "[DONE]") { ended = true; continue; }
        const auto document = QJsonDocument::fromJson(data);
        if (!document.isObject() || document.object().contains("error"))
            throw std::runtime_error("The word response was interrupted. Try again.");
        const auto choices = document.object().value("choices").toArray();
        if (choices.isEmpty()) continue; // Usage-only frame.
        if (ended || choices.size() != 1) throw std::runtime_error("Invalid word response stream.");
        const auto choice = choices.first().toObject();
        const auto reason = choice.value("finish_reason").toString();
        if (!reason.isEmpty() && reason != "stop")
            throw std::runtime_error("The word response ended early. Try again.");
        if (reason == "stop") stopped = true;
        content += choice.value("delta").toObject().value("content").toString();
        if (content.size() > 64000) throw std::runtime_error("The word response exceeded its size limit.");
    }
}
QMap<QString, QVariantMap> WordBatch::entries() const {
    QMap<QString, QVariantMap> result;
    // The fixed array envelope lets us publish a finished card before the rest
    // of the JSON document arrives, including across UTF-8 and escape boundaries.
    static const QRegularExpression envelope(QStringLiteral("^\\s*\\{\\s*\"entries\"\\s*:\\s*\\["));
    const auto match = envelope.match(content);
    if (!match.hasMatch()) return result;
    qsizetype position = match.capturedEnd();
    while (position < content.size()) {
        while (position < content.size() && content.at(position).isSpace()) ++position;
        if (position >= content.size() || content.at(position) != '{') break;
        const auto start = position;
        bool quoted = false, escaped = false;
        int depth = 0;
        for (; position < content.size(); ++position) {
            const auto character = content.at(position);
            if (quoted) {
                if (escaped) escaped = false;
                else if (character == '\\') escaped = true;
                else if (character == '"') quoted = false;
            } else if (character == '"') quoted = true;
            else if (character == '{') ++depth;
            else if (character == '}' && --depth == 0) { ++position; break; }
        }
        if (depth != 0 || quoted) break;
        const auto entry = QJsonDocument::fromJson(content.mid(start, position - start).toUtf8()).object();
        const auto word = entry.value("word").toString();
        const auto meaning = entry.value("meaning").toString().trimmed();
        const auto part = entry.value("partOfSpeech").toString().trimmed();
        if (!expectedWords.contains(word) || result.contains(word) || meaning.isEmpty() || meaning.size() > 100 ||
            !entry.value("partOfSpeech").isString() || part.size() > 40)
            throw std::runtime_error("The word list contained an invalid entry. Try again.");
        result.insert(word, {{"meaning", meaning}, {"partOfSpeech", part}});
        while (position < content.size() && content.at(position).isSpace()) ++position;
        if (position >= content.size() || content.at(position++) != ',') break;
    }
    return result;
}
bool WordBatch::complete() const {
    return ended && stopped && QJsonDocument::fromJson(content.toUtf8()).isObject() &&
        entries().size() == expectedWords.size();
}
