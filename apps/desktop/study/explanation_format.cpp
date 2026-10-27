#include "explanation_format.h"
#include "explanation_response.h"
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QTextList>
#include <QSet>
#include <QMap>
#include <QUrl>

namespace {
class OfflineDocument : public QTextDocument {
protected:
    QVariant loadResource(int, const QUrl &) override { return QByteArray(); }
};

void parseMarkdown(QTextDocument &document, const QString &markdown) {
    document.setMarkdown(ExplanationResponse::markdown(markdown).left(65536),
                         QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) |
                             QTextDocument::MarkdownNoHTML);
}

// The section positions work with translated headings too. Only top-level
// breakdown bullets participate; headings and nested examples are not entries.
struct BreakdownIndex {
    int translationStart = -1;
    int translationEnd = -1;
    QMap<QString, QStringList> entries;
};

BreakdownIndex indexBreakdown(const QTextDocument &document) {
    BreakdownIndex index;
    int section = 0;
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel()) {
            ++section;
            if (section == 1) index.translationStart = block.position() + block.length();
            if (section == 2) index.translationEnd = block.position();
        } else if (section == 2 && block.textList() && block.textList()->format().indent() == 1) {
            QString sourceWord;
            for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
                const auto text = fragment.fragment();
                if (text.charFormat().fontWeight() < QFont::Bold) break;
                sourceWord += text.text();
            }
            if (!sourceWord.trimmed().isEmpty())
                index.entries[sourceWord.trimmed().normalized(QString::NormalizationForm_C).toCaseFolded()].append(block.text());
        }
    }
    return index;
}

QString resolveBreakdown(const BreakdownIndex &index, const QString &link) {
    const QString prefix = QStringLiteral("neden-breakdown:");
    if (!link.startsWith(prefix) || link.size() > 2048) return {};
    const auto words = link.mid(prefix.size()).split(',');
    if (words.size() > 32) return {};
    QStringList selected;
    QSet<QString> included;
    for (const auto &word : words) {
        const auto key = QUrl::fromPercentEncoding(word.toUtf8()).trimmed().normalized(QString::NormalizationForm_C).toCaseFolded();
        if (!index.entries.contains(key)) return {};
        if (!included.contains(key)) {
            selected.append(index.entries.value(key));
            included.insert(key);
        }
    }
    return selected.join("\n\n");
}
} // namespace

QString ExplanationFormat::breakdown(const QString &markdown, const QString &link) {
    OfflineDocument document;
    parseMarkdown(document, markdown);
    const auto index = indexBreakdown(document);
    // Require an actual translation link, not just a caller-supplied index.
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        if (block.position() < index.translationStart || block.position() >= index.translationEnd) continue;
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment)
            if (fragment.fragment().charFormat().anchorHref() == link)
                return resolveBreakdown(index, link);
    }
    return {};
}

QString ExplanationFormat::render(const QString &markdown, const QColor &linkColor) {

    // 1. Parse Markdown without HTML or access to local or remote resources.
    OfflineDocument document;
    QFont bodyFont("Inter");
    bodyFont.setPixelSize(15);
    document.setDefaultFont(bodyFont);
    parseMarkdown(document, markdown);
    const auto breakdownIndex = indexBreakdown(document);

    // 2. Replace images with labels. Only validated, local translation references
    // survive; provider-supplied URLs never become navigation actions.
    QList<QTextCursor> fragments;
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        for (auto iterator = block.begin(); !iterator.atEnd(); ++iterator) {
            const auto fragment = iterator.fragment();
            if (!fragment.isValid())
                continue;

            QTextCursor cursor(&document);
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            fragments.append(cursor);
        }
    }
    for (auto iterator = fragments.rbegin(); iterator != fragments.rend(); ++iterator) {
        auto cursor = *iterator;
        auto format = cursor.charFormat();
        if (format.isImageFormat()) {
            const auto label = format.property(QTextFormat::ImageAltText).toString();
            cursor.insertText(label, QTextCharFormat());
            continue;
        }

        if (format.isAnchor()) {
            const bool translationReference = cursor.position() >= breakdownIndex.translationStart &&
                cursor.anchor() < breakdownIndex.translationEnd &&
                !resolveBreakdown(breakdownIndex, format.anchorHref()).isEmpty();
            format.setAnchor(translationReference);
            if (!translationReference) format.setAnchorHref({});
            format.clearProperty(QTextFormat::ForegroundBrush);
            if (translationReference) format.setForeground(linkColor);
            format.setFontUnderline(translationReference);
            cursor.setCharFormat(format);
        }
    }

    // 3. Give streamed explanations consistent paragraph rhythm and compact headings.
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block);
        auto format = block.blockFormat();
        format.setLineHeight(155, QTextBlockFormat::ProportionalHeight);
        format.setTopMargin(format.headingLevel() ? 16 : 0);
        format.setBottomMargin(block.textList() ? 4 : 10);
        cursor.setBlockFormat(format);
        if (format.headingLevel()) {
            cursor.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat heading;
            heading.setProperty(QTextFormat::FontPixelSize, 17);
            heading.setFontWeight(QFont::DemiBold);
            cursor.mergeCharFormat(heading);
        }
    }
    return document.toHtml();
}
