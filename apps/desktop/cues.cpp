#include "cues.h"
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>

QString subtitlePlainText(const QString &input) {
    // SRT presentation tags do not belong in selections, search, or model context.
    // Unknown tags remain literal text; no subtitle content is interpreted as HTML.
    static const QRegularExpression formatting(
        "</?(?:i|b|u|s)\\s*>|</?font(?:\\s+[^<>]*)?>",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression lineBreak("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption);
    QString text = input;
    text.replace(lineBreak, "\n");
    text.remove(formatting);
    text.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"")
        .replace("&#39;", "'").replace("&nbsp;", " ").replace("&amp;", "&");
    return text;
}

QVector<Cue> parseSrt(const QString &input) {

    // 1. Bound the input and split normalized subtitle blocks.
    if (input.toUtf8().size() > 8 * 1024 * 1024) {
        throw std::runtime_error("Subtitle file exceeds 8 MiB.");
    }

    QString normalized = input;
    normalized.replace("\r\n", "\n");
    normalized.remove(QChar(0xfeff));
    const auto blocks = normalized.trimmed().split(QRegularExpression("\\n[ \\t]*\\n"));
    const QRegularExpression timing("^(\\d{2}):(\\d{2}):(\\d{2})[,\\.](\\d{3}) --> (\\d{2}):(\\d{2}):(\\d{2})[,\\.](\\d{3})$");
    QVector<Cue> result;

    // 2. Validate complete timings and retain subtitle content as plain text.
    for (const auto &block : blocks) {
        auto lines = block.split('\n');
        if (lines.size() < 3) {
            throw std::runtime_error("Expected numbered SRT blocks.");
        }

        auto match = timing.match(lines[1].trimmed());
        if (!match.hasMatch()) {
            throw std::runtime_error("Invalid SRT timestamp.");
        }

        auto milliseconds = [&match](int offset) -> qint64 {
            const int minutes = match.captured(offset + 1).toInt();
            const int seconds = match.captured(offset + 2).toInt();
            if (minutes > 59 || seconds > 59) {
                throw std::runtime_error("Invalid timestamp component.");
            }

            return ((match.captured(offset).toLongLong() * 60 + minutes) * 60 + seconds) * 1000
                + match.captured(offset + 3).toInt();
        };
        Cue cue{milliseconds(1), milliseconds(5), subtitlePlainText(lines.mid(2).join('\n')).trimmed(), {}};
        if (cue.endMs <= cue.startMs || cue.text.isEmpty() || cue.text.size() > 4000) {
            throw std::runtime_error("Invalid subtitle cue.");
        }

        result.append(cue);
        if (result.size() > 50000) {
            throw std::runtime_error("Too many subtitle cues.");
        }
    }

    // 3. Sort once so playback can use binary lookup.
    std::stable_sort(result.begin(), result.end(), [](const Cue &a, const Cue &b) {
        return a.startMs < b.startMs;
    });
    return result;
}

int cueAt(const QVector<Cue> &cues, qint64 positionMs) {
    const auto next = std::upper_bound(cues.begin(), cues.end(), positionMs,
        [](qint64 position, const Cue &cue) { return position < cue.startMs; });
    if (next == cues.begin()) {
        return -1;
    }

    const int index = static_cast<int>(next - cues.begin()) - 1;
    return positionMs < cues[index].endMs ? index : -1;
}

int CueModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : visible.size(); }
QVariant CueModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= visible.size()) {
        return {};
    }

    const int original = visible[index.row()];
    const auto &cue = cues[original];
    switch (role) {
        case Text: return cue.text;
        case Translation: return cue.translation;
        case Start: return cue.startMs / 1000.0;
        case CueIndex: return original;
        default: return {};
    }
}
QHash<int, QByteArray> CueModel::roleNames() const {
    return {{Text, "cueText"}, {Translation, "translation"}, {Start, "startSeconds"}, {CueIndex, "cueIndex"}};
}
void CueModel::replace(QVector<Cue> next) {
    beginResetModel();
    cues = std::move(next);
    currentQuery.clear();
    rebuildSearchKeys();
    visible.clear();
    for (int index = 0; index < cues.size(); ++index) {
        visible.append(index);
    }
    endResetModel();
    emit countChanged();
}
QString CueModel::searchKey(const QString &text) const {

    // 1. Treat composed and decomposed spellings as the same search text.
    QString normalized = text.normalized(QString::NormalizationForm_C);
    if (language == Turkish) {
        // Unicode's Turkic case-fold pairs; independent of the OS locale or ICU.
        normalized.replace(QChar(0x0049), QChar(0x0131));
        normalized.replace(QChar(0x0130), QChar(0x0069));
    }

    // 2. Fold remaining case differences without discarding accents or changing display text.
    return normalized.toCaseFolded().normalized(QString::NormalizationForm_C);
}
void CueModel::rebuildSearchKeys() {
    searchKeys.clear();
    searchKeys.reserve(cues.size());
    for (const auto &cue : cues) {
        searchKeys.append(searchKey(cue.text));
    }
}
void CueModel::setSearchLanguage(SearchLanguage value) {
    if (value == language || (value != Unicode && value != Turkish)) {
        return;
    }

    language = value;
    rebuildSearchKeys();
    search(currentQuery);
    emit searchLanguageChanged();
}
void CueModel::search(const QString &query) {
    currentQuery = query;
    const QString key = searchKey(query);
    beginResetModel();
    visible.clear();
    for (int index = 0; index < cues.size(); ++index) {
        if (searchKeys[index].contains(key)) {
            visible.append(index);
        }
    }
    endResetModel();
    emit countChanged();
}
