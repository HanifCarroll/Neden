#include "subtitle_appearance.h"
#include <QDir>
#include <QRegularExpression>

SubtitleAppearance::SubtitleAppearance(const QString &directory, QObject *parent)
    : QObject(parent), settings(directory + "/appearance.ini", QSettings::IniFormat) {
    sourceSize = qBound(16, settings.value("primarySize", 28).toInt(), 64);
    translationSize = qBound(12, settings.value("secondarySize", 24).toInt(), 56);
    margin = qBound(8, settings.value("bottomMargin", 36).toInt(), 160);
    outlined = settings.value("outline", true).toBool();
    boxed = settings.value("background", false).toBool();
    semibold = settings.value("bold", true).toBool();
    const auto savedColor = settings.value("textColor", "#ffffff").toString();
    if (QRegularExpression("^#[0-9a-fA-F]{6}$").match(savedColor).hasMatch()) color = savedColor;
}

bool SubtitleAppearance::configure(int primary, int secondary, int bottom, bool outline,
                                   bool background, bool bold, const QString &textColor) {

    // 1. Bound visual preferences without interpreting arbitrary style or rich text.
    if (primary < 16 || primary > 64 || secondary < 12 || secondary > 56 || bottom < 8 || bottom > 160
        || !QRegularExpression("^#[0-9a-fA-F]{6}$").match(textColor).hasMatch()) {
        return false;
    }

    // 2. Save appearance separately from media history and track choices.
    settings.setValue("primarySize", primary);
    settings.setValue("secondarySize", secondary);
    settings.setValue("bottomMargin", bottom);
    settings.setValue("outline", outline);
    settings.setValue("background", background);
    settings.setValue("bold", bold);
    settings.setValue("textColor", textColor);
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;

    sourceSize = primary;
    translationSize = secondary;
    margin = bottom;
    outlined = outline;
    boxed = background;
    semibold = bold;
    color = textColor;
    emit changed();
    return true;
}

void SubtitleAppearance::reset() {
    configure(28, 24, 36, true, false, true, "#ffffff");
}
