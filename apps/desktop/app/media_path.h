#pragma once
#include <QFileInfo>
#include <QString>

inline QString mediaPathIdentity(const QString &path) {
    if (path.isEmpty())
        return {};

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
