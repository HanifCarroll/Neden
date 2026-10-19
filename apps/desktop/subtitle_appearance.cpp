#include "subtitle_appearance.h"
#include <QDir>
#include <QRegularExpression>
#include <cmath>

SubtitleAppearance::SubtitleAppearance(const QString &directory, QObject *parent)
    : QObject(parent), settings(directory + "/appearance.ini", QSettings::IniFormat) {
    sourceSize = qBound(16, settings.value("primarySize", 28).toInt(), 64);
    translationSize = qBound(12, settings.value("secondarySize", 24).toInt(), 56);
    margin = qBound(8, settings.value("bottomMargin", 36).toInt(), 160);
    outlined = settings.value("outline", true).toBool();
    boxed = settings.value("background", false).toBool();
    semibold = settings.value("bold", true).toBool();
    QVariantMap values = snapshot();
    for (auto it = values.begin(); it != values.end(); ++it) *it = settings.value(it.key(), it.value());
    preview(values);
    savedValues = snapshot();
}

bool SubtitleAppearance::configure(int primary, int secondary, int bottom, bool outline,
                                   bool background, bool bold, const QString &textColor) {

    // 1. Bound visual preferences without interpreting arbitrary style or rich text.
    if (primary < 16 || primary > 64 || secondary < 12 || secondary > 56 || bottom < 8 || bottom > 160
        || !QRegularExpression("^#[0-9a-fA-F]{6}$").match(textColor).hasMatch()) {
        return false;
    }

    // 2. Apply the validated appearance and persist only on explicit confirmation.
    auto values = snapshot();
    values.insert("primarySize", primary);
    values.insert("secondarySize", secondary);
    values.insert("bottomMargin", bottom);
    values.insert("outline", outline);
    values.insert("background", background);
    values.insert("bold", bold);
    values.insert("textColor", textColor);
    return preview(values) && applyPreview();
}

QVariantMap SubtitleAppearance::snapshot() const {
    return {{"primarySize", sourceSize}, {"secondarySize", translationSize}, {"bottomMargin", margin},
        {"outline", outlined}, {"background", boxed}, {"bold", semibold}, {"textColor", color},
        {"secondaryColor", translationColor}, {"gap", trackGap}, {"reversed", reverseOrder},
        {"selectable", selectionEnabled}, {"bottomPercent", relativeBottom}};
}

bool SubtitleAppearance::preview(const QVariantMap &values) {

    // 1. Reject invalid drafts before changing the live preview.
    const int primary = values.value("primarySize").toInt();
    const int secondary = values.value("secondarySize").toInt();
    const int bottom = values.value("bottomMargin").toInt();
    const int gap = values.value("gap").toInt();
    const double percent = values.value("bottomPercent").toDouble();
    const QRegularExpression hex("^#[0-9a-fA-F]{6}$");
    if (primary < 16 || primary > 64 || secondary < 12 || secondary > 56 || bottom < 8 || bottom > 160
        || gap < 0 || gap > 80 || !std::isfinite(percent) || percent < 0 || percent > 40
        || !hex.match(values.value("textColor").toString()).hasMatch()
        || !hex.match(values.value("secondaryColor").toString()).hasMatch()) return false;

    // 2. Publish a reversible, non-persistent preview to the player.
    sourceSize = primary;
    translationSize = secondary;
    margin = bottom;
    outlined = values.value("outline").toBool();
    boxed = values.value("background").toBool();
    semibold = values.value("bold").toBool();
    color = values.value("textColor").toString();
    translationColor = values.value("secondaryColor").toString();
    trackGap = gap;
    reverseOrder = values.value("reversed").toBool();
    selectionEnabled = values.value("selectable").toBool();
    relativeBottom = percent;
    emit changed();
    return true;
}

bool SubtitleAppearance::applyPreview() {
    const auto values = snapshot();
    for (auto it = values.begin(); it != values.end(); ++it) settings.setValue(it.key(), it.value());
    settings.sync();
    if (settings.status() != QSettings::NoError) return false;

    savedValues = values;
    return true;
}

void SubtitleAppearance::cancelPreview() { preview(savedValues); }

void SubtitleAppearance::reset() {
    preview({{"primarySize", 28}, {"secondarySize", 24}, {"bottomMargin", 36},
        {"outline", true}, {"background", false}, {"bold", true}, {"textColor", "#ffffff"},
        {"secondaryColor", "#ffffff"}, {"gap", 12}, {"reversed", false}, {"selectable", true}, {"bottomPercent", 0}});
    applyPreview();
}
