#include "profile_store.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QDateTime>
#include <stdexcept>
namespace {
bool validCards(const QJsonValue &value) {
    if (!value.isArray() || value.toArray().size() > 10000)
        return false;

    for (const auto entry : value.toArray()) {
        if (!entry.isObject())
            return false;

        const auto card = entry.toObject();
        for (const auto *field : {"front", "back", "context", "media", "due"}) {
            if (!card.value(field).isString())
                return false;
        }
        if (!card.value("seconds").isDouble() || card.value("seconds").toDouble() < 0 ||
            !card.value("reviews").isDouble() || card.value("reviews").toDouble() < 0 ||
            !QDateTime::fromString(card.value("due").toString(), Qt::ISODate).isValid())
            return false;
    }
    return true;
}
} // namespace

ProfileStore::ProfileStore(QString directory) : storage(std::move(directory)) {
    QFile file(storage + "/study.json");
    if (!file.exists())
        return;
    QJsonParseError parse;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
        valid = false;
    else {
        const auto data = QJsonDocument::fromJson(file.readAll(), &parse);
        valid = parse.error == QJsonParseError::NoError && data.isObject() &&
                data.object().value("version").toInt() == 1 && validCards(data.object().value("cards"));
        if (valid)
            document = data.object();
    }
    if (!valid)
        errorText = "Saved study data cannot be read. It has been preserved; saving is disabled.";
}
bool ProfileStore::update(const QJsonObject &fields) {
    if (!valid)
        return false;
    auto next = document;
    for (auto it = fields.begin(); it != fields.end(); ++it)
        next.insert(it.key(), it.value());
    const auto bytes = QJsonDocument(next).toJson();
    QDir().mkpath(storage);
    QSaveFile file(storage + "/study.json");
    if (bytes.size() > 8 * 1024 * 1024 || !file.open(QIODevice::WriteOnly) ||
        file.write(bytes) != bytes.size() || !file.commit()) {
        errorText = "Could not save study data. The previous file is preserved.";
        return false;
    }
    document = next;
    errorText.clear();
    return true;
}
