#include "explanation_format.h"
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextImageFormat>

namespace {
class OfflineDocument : public QTextDocument {
protected:
    QVariant loadResource(int, const QUrl &) override { return QByteArray(); }
};
} // namespace

QString ExplanationFormat::render(const QString &markdown) {

    // 1. Parse Markdown without HTML or access to local or remote resources.
    OfflineDocument document;
    QFont bodyFont("Inter");
    bodyFont.setPixelSize(15);
    document.setDefaultFont(bodyFont);
    document.setMarkdown(markdown, QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) |
                                       QTextDocument::MarkdownNoHTML);

    // 2. Replace image objects with their labels and remove link destinations.
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
            format.setAnchor(false);
            format.setAnchorHref({});
            format.clearProperty(QTextFormat::ForegroundBrush);
            format.setFontUnderline(false);
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
