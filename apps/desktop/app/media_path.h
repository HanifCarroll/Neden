#pragma once
#include <QFileInfo>
#include <QString>
#include <QUrl>

inline QString mediaPathIdentity(const QString &path) {
    if (path.isEmpty())
        return {};

    const QUrl url(path);
    if (url.scheme() == "https" || url.scheme() == "http")
        return url.toString(QUrl::FullyEncoded);
    const QFileInfo file(path);
    const QString canonical = file.canonicalFilePath();
    const QString absolute = canonical.isEmpty() ? file.absoluteFilePath() : canonical;
#ifdef Q_OS_MACOS
    // Finder and file pickers can use different Unicode forms for the same Mac file.
    return absolute.normalized(QString::NormalizationForm_C);
#else
    return absolute;
#endif
}
