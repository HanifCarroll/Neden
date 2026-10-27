#include "explanation_response.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUrl>

// Provider JSON is kept as conversation evidence. Only complete JSON values are
// shown while streaming; unfinished escapes or objects never become UI text.
namespace ExplanationResponse {
namespace {
qsizetype valueEnd(const QString &text, qsizetype start) {
    bool quoted = false, escaped = false;
    int depth = 0;
    for (qsizetype position = start; position < text.size(); ++position) {
        const QChar character = text.at(position);
        if (quoted) {
            if (escaped) escaped = false;
            else if (character == '\\') escaped = true;
            else if (character == '"') {
                quoted = false;
                if (!depth) return position + 1;
            }
        } else if (character == '"') quoted = true;
        else if (character == '{' || character == '[') ++depth;
        else if (character == '}' || character == ']') {
            if (--depth == 0) return position + 1;
        }
    }
    return -1;
}
void skipSpace(const QString &text, qsizetype &position) {
    while (position < text.size() && text.at(position).isSpace()) ++position;
}
QJsonValue decodeValue(const QString &value) {
    const auto array = QJsonDocument::fromJson(("[" + value + "]").toUtf8()).array();
    return array.isEmpty() ? QJsonValue() : array.first();
}
QJsonObject availableFields(const QString &text) {
    const auto complete = QJsonDocument::fromJson(text.toUtf8());
    if (complete.isObject()) return complete.object();
    QJsonObject fields;
    qsizetype position = 1;
    while (position < text.size()) {
        skipSpace(text, position);
        if (position >= text.size() || text.at(position) != '"') break;
        const auto keyEnd = valueEnd(text, position);
        if (keyEnd < 0) break;
        const auto key = decodeValue(text.mid(position, keyEnd - position)).toString();
        position = keyEnd;
        skipSpace(text, position);
        if (position >= text.size() || text.at(position++) != ':') break;
        skipSpace(text, position);
        if (position >= text.size()) break;
        const auto end = valueEnd(text, position);
        if (end < 0) {
            if (text.at(position) == '[') {
                QJsonArray items;
                ++position;
                while (position < text.size()) {
                    skipSpace(text, position);
                    if (position >= text.size() || text.at(position) != '{') break;
                    const auto itemEnd = valueEnd(text, position);
                    if (itemEnd < 0) break;
                    const auto item = decodeValue(text.mid(position, itemEnd - position));
                    if (!item.isObject()) break;
                    items.append(item);
                    position = itemEnd;
                    skipSpace(text, position);
                    if (position >= text.size() || text.at(position++) != ',') break;
                }
                fields.insert(key, items);
            }
            break;
        }
        fields.insert(key, decodeValue(text.mid(position, end - position)));
        position = end;
        skipSpace(text, position);
        if (position >= text.size() || text.at(position++) != ',') break;
    }
    return fields;
}
QString escapedMarkdown(const QString &text) {
    QString escaped;
    for (const auto character : text) {
        if (QStringLiteral("\\`*_{}[]<>#!|").contains(character)) escaped += '\\';
        escaped += character;
    }
    return escaped;
}
} // namespace

QString markdown(const QString &response, bool includeReferences) {
    const auto text = response.trimmed();
    if (!text.startsWith('{')) return response; // Legacy answers and Markdown follow-ups.
    const auto fields = availableFields(text.left(65536));
    const auto translation = fields.value("translation").toString();
    const auto alignment = fields.value("alignment").toArray();
    const auto breakdown = fields.value("breakdown").toArray();
    QString result;
    if (!translation.isEmpty()) {
        result = "### " + escapedMarkdown(fields.value("translationHeading").toString("Translation")) + "\n\n";
        qsizetype cursor = 0;
        for (const auto &value : alignment) {
            const auto segment = value.toObject();
            const auto original = segment.value("text").toString();
            if (original.isEmpty()) continue;
            const auto position = translation.indexOf(original, cursor);
            if (position < 0) continue;
            result += escapedMarkdown(translation.mid(cursor, position - cursor));
            QStringList references;
            for (const auto &source : segment.value("source").toArray()) {
                if (source.isString() && !source.toString().trimmed().isEmpty())
                    references.append(QString::fromLatin1(QUrl::toPercentEncoding(source.toString())));
            }
            if (includeReferences && !references.isEmpty()) {
                result += "[" + escapedMarkdown(original) + "](neden-breakdown:" + references.join(',') + ")";
            } else result += escapedMarkdown(original);
            cursor = position + original.size();
        }
        result += escapedMarkdown(translation.mid(cursor));
        result += "\n\n";
    }
    if (!breakdown.isEmpty()) {
        result += "### " + escapedMarkdown(fields.value("breakdownHeading").toString("Breakdown")) + "\n\n";
        for (const auto &value : breakdown) {
            const auto entry = value.toObject();
            const auto word = entry.value("word").toString();
            bool containsWord = false;
            for (const auto character : word.toUcs4()) containsWord = containsWord || QChar::isLetterOrNumber(character);
            if (!containsWord || entry.value("text").toString().isEmpty()) continue;
            result += "- **" + escapedMarkdown(word) + "** — " +
                escapedMarkdown(entry.value("text").toString()) + "\n";
        }
    }
    return result;
}
bool isComplete(const QString &response) {
    if (!response.trimmed().startsWith('{')) return !response.trimmed().isEmpty();
    const auto document = QJsonDocument::fromJson(response.toUtf8());
    if (!document.isObject()) return false;
    const auto fields = document.object();
    if (fields.value("translation").toString().trimmed().isEmpty()) return false;
    for (const auto &name : {"alignment", "breakdown"}) {
        const auto entries = fields.value(name).toArray();
        if (entries.isEmpty() || entries.size() > 1000) return false;
        for (const auto &value : entries) {
            const auto entry = value.toObject();
            if (entry.value("text").toString().trimmed().isEmpty()) return false;
            if (QString(name) == "breakdown" && entry.value("word").toString().trimmed().isEmpty()) return false;
            if (QString(name) == "alignment" && !entry.value("source").isArray()) return false;
        }
    }
    return true;
}
} // namespace ExplanationResponse
