#include "media_queue.h"
#include "media_path.h"
#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

QList<QUrl> MediaQueue::expand(const QList<QUrl> &urls) {
    QList<QUrl> result;
    QSet<QUrl> seen;
    const QStringList extensions{"mp4",  "mkv", "webm", "mov", "m4v", "avi", "mpg",
                                 "mpeg", "ts",  "m2ts", "wmv", "ogv", "flv"};
    // The C locale does not implement numeric collation on Linux.
    const auto locale = QLocale::system();
    QCollator collator{locale.language() == QLocale::C ? QLocale(QLocale::English, QLocale::UnitedStates)
                                                      : locale};
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    for (const auto &url : urls) {
        QList<QUrl> candidates;
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir()) {
            auto entries = QDir(url.toLocalFile()).entryInfoList(QDir::Files | QDir::Readable, QDir::NoSort);
            std::sort(entries.begin(), entries.end(),
                      [&collator](const QFileInfo &left, const QFileInfo &right) {
                          return collator.compare(left.fileName(), right.fileName()) < 0;
                      });
            for (const auto &entry : entries)
                if (extensions.contains(entry.suffix().toLower()))
                    candidates << QUrl::fromLocalFile(entry.absoluteFilePath());
        } else if (url.isLocalFile() || url.scheme() == "https" || url.scheme() == "http") {
            candidates << url;
        }
        for (const auto &candidate : candidates) {
            const auto normalized = candidate.isLocalFile()
                                        ? QUrl::fromLocalFile(mediaPathIdentity(candidate.toLocalFile()))
                                        : candidate;
            if (!seen.contains(normalized) && result.size() < 10000) {
                result << normalized;
                seen.insert(normalized);
            }
        }
    }
    return result;
}
QVariantList MediaQueue::entries() const {
    QVariantList result;
    for (int index = 0; index < files.size(); ++index) {
        const auto &url = files[index];
        result << QVariantMap{
            {"title", url.isLocalFile() ? QFileInfo(url.toLocalFile()).fileName() : url.host() + url.path()},
            {"url", url},
            {"current", !currentEntryRemoved && index == selectedIndex}};
    }
    return result;
}
void MediaQueue::replace(const QList<QUrl> &urls) {
    files = expand(urls);
    selectedIndex = -1;
    currentEntryRemoved = false;
    emit changed();
}
void MediaQueue::append(const QList<QUrl> &urls) {
    for (const auto &url : expand(urls))
        if (!files.contains(url) && files.size() < 10000)
            files << url;
    emit changed();
}
void MediaQueue::setCurrent(const QUrl &url) {
    currentEntryRemoved = false;
    selectedIndex = files.indexOf(url);
    if (selectedIndex < 0) {
        files = {url};
        selectedIndex = 0;
    }
    emit changed();
}
void MediaQueue::play(int index) {
    if (index < 0 || index >= files.size())
        return;
    currentEntryRemoved = false;
    selectedIndex = index;
    emit changed();
    emit openRequested(files[index]);
}
void MediaQueue::next() {
    if (hasNext())
        play(selectedIndex + 1);
}
void MediaQueue::previous() {
    if (hasPrevious())
        play(currentEntryRemoved ? selectedIndex : selectedIndex - 1);
}
void MediaQueue::setAutoplay(bool value) {
    advanceAutomatically = value;
    emit changed();
}
void MediaQueue::remove(int index) {
    if (index < 0 || index >= files.size())
        return;
    if (index == selectedIndex && !currentEntryRemoved)
        currentEntryRemoved = true;
    files.removeAt(index);
    if (index <= selectedIndex)
        --selectedIndex;
    emit changed();
}
void MediaQueue::move(int from, int to) {
    if (from < 0 || to < 0 || from >= files.size() || to >= files.size())
        return;
    const auto current = selectedIndex >= 0 ? files[selectedIndex] : QUrl();
    files.move(from, to);
    selectedIndex = files.indexOf(current);
    emit changed();
}
void MediaQueue::clear() {
    files.clear();
    selectedIndex = -1;
    currentEntryRemoved = false;
    emit changed();
}
