#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>

enum class SubtitleRole { Primary, Secondary };
// A value snapshot. Its media identity stays local; requests contain language context only.
struct StudyTarget {
    QString mediaPath, selection, sourceCue, translation, sourceLanguage, explanationLanguage;
    QStringList before, after, secondary;
    SubtitleRole role = SubtitleRole::Primary;
    int cueIndex = -1;
    qint64 positionMs = 0, startMs = 0, endMs = 0;
    static StudyTarget fromJson(const QJsonObject &value) {
        StudyTarget target;
        target.selection = value.value("selection").toString();
        target.role = value.value("selectedTrack").toString() == "secondary" ? SubtitleRole::Secondary
                                                                             : SubtitleRole::Primary;
        target.sourceCue = value.value("sourceCue").toString();
        target.sourceLanguage = value.value("sourceLanguage").toString();
        target.explanationLanguage = value.value("explanationLanguage").toString();
        target.cueIndex = value.value("cueIndex").toInt(-1);
        target.startMs = qint64(value.value("cueStartMs").toDouble());
        target.endMs = qint64(value.value("cueEndMs").toDouble());
        target.positionMs = target.startMs;
        for (const auto &item : value.value("before").toArray())
            target.before.append(item.toString());
        for (const auto &item : value.value("after").toArray())
            target.after.append(item.toString());
        for (const auto &item : value.value("secondary").toArray())
            target.secondary.append(item.toString());
        return target;
    }
    QJsonObject json() const {
        return {{"selection", selection},
                {"sourceCue", sourceCue},
                {"clickedSubtitle", sourceCue},
                {"before", QJsonArray::fromStringList(before)},
                {"after", QJsonArray::fromStringList(after)},
                {"secondary", QJsonArray::fromStringList(secondary)},
                {"sourceLanguage", sourceLanguage},
                {"explanationLanguage", explanationLanguage},
                {"selectedTrack", role == SubtitleRole::Primary ? "primary" : "secondary"},
                {"cueIndex", cueIndex},
                {"cueStartMs", startMs},
                {"cueEndMs", endMs}};
    }
};
