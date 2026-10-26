#pragma once
#include <QJsonObject>

// Only language evidence crosses the hosted boundary. Paths and playback metadata stay local.
inline QJsonObject hostedLanguageContext(const QJsonObject &context) {
    QJsonObject result;
    for (const auto *key : {"selection", "clickedSubtitle", "sourceCue", "sourceLanguage",
                            "explanationLanguage", "before", "after", "secondary"})
        if (context.contains(key)) result.insert(key, context.value(key));
    return result;
}
