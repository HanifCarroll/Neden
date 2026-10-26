#include "support_diagnostics.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QSysInfo>

QString SupportDiagnostics::report(bool hasMedia, bool paused, const QVariantMap &information) const {
    QStringList lines{
        QStringLiteral("Neden: ") + QStringLiteral(NEDEN_VERSION),
        QStringLiteral("Qt: ") + QString::fromLatin1(qVersion()),
        QStringLiteral("OS: ") + QSysInfo::prettyProductName(),
        QStringLiteral("Architecture: ") + QSysInfo::currentCpuArchitecture(),
        QStringLiteral("Window system: ") + QGuiApplication::platformName(),
        QStringLiteral("Playback: ") + (hasMedia ? (paused ? QStringLiteral("paused") : QStringLiteral("playing")) : QStringLiteral("no media"))};
    if (hasMedia) {
        // Numeric fields cannot smuggle filenames, metadata strings, or tokens into reports.
        const auto video = information.value("Video").toMap();
        const auto audio = information.value("Audio").toMap();
        const int width = video.value("w").toInt();
        const int height = video.value("h").toInt();
        const int sampleRate = audio.value("samplerate").toInt();
        if (width > 0 && height > 0)
            lines << QStringLiteral("Video: %1 × %2 pixels").arg(width).arg(height);
        if (sampleRate > 0)
            lines << QStringLiteral("Audio sample rate: %1 Hz").arg(sampleRate);
    }
    return lines.join('\n');
}
void SupportDiagnostics::copy(bool hasMedia, bool paused, const QVariantMap &information) const {
    QGuiApplication::clipboard()->setText(report(hasMedia, paused, information));
}
