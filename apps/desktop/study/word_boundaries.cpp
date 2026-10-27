#include "word_boundaries.h"
#include <memory>
#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#else
#ifdef Q_OS_WIN
#include <icu.h>
#else
#include <unicode/ubrk.h>
#endif
#endif

namespace {
QVector<WordSpan> segmentWords(const QString &text) {
    QVector<WordSpan> words;
    if (text.isEmpty() || text.size() > 32000)
        return words;
#ifdef Q_OS_MACOS
    const auto string = text.toCFString();
    const auto language = CFStringTokenizerCopyBestStringLanguage(string, CFRangeMake(0, text.size()));
    const auto locale = language ? CFLocaleCreate(kCFAllocatorDefault, language) : nullptr;
    const auto tokenizer = CFStringTokenizerCreate(kCFAllocatorDefault, string,
        CFRangeMake(0, text.size()), kCFStringTokenizerUnitWord, locale);
    if (tokenizer) {
        while (CFStringTokenizerAdvanceToNextToken(tokenizer) != kCFStringTokenizerTokenNone) {
            const auto range = CFStringTokenizerGetCurrentTokenRange(tokenizer);
            words.append({static_cast<int>(range.location), static_cast<int>(range.location + range.length)});
        }
        CFRelease(tokenizer);
    }
    if (locale) CFRelease(locale);
    if (language) CFRelease(language);
    CFRelease(string);
#else
    UErrorCode status = U_ZERO_ERROR;
    const std::unique_ptr<UBreakIterator, decltype(&ubrk_close)> iterator(
        ubrk_open(UBRK_WORD, "und", reinterpret_cast<const UChar *>(text.utf16()),
                  static_cast<int32_t>(text.size()), &status), &ubrk_close);
    if (U_FAILURE(status) || !iterator)
        return words;
    int start = ubrk_first(iterator.get());
    for (int end = ubrk_next(iterator.get()); end != UBRK_DONE; end = ubrk_next(iterator.get())) {
        if (ubrk_getRuleStatus(iterator.get()) != UBRK_WORD_NONE)
            words.append({start, end});
        start = end;
    }
#endif
    return words;
}

} // namespace

QVector<WordSpan> dictionaryWordBoundaries(const QString &text) {
    // Hover hit-testing revisits the same subtitle many times. Retain only the
    // most recent bounded sentence per thread, without accumulating a corpus.
    thread_local QString previousText;
    thread_local QVector<WordSpan> previousWords;
    if (text.size() > 32000) return {};
    if (text != previousText) {
        previousWords = segmentWords(text);
        previousText = text;
    }
    return previousWords;
}
