#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
// One atomic document preserves the established profile format across independent features.
class ProfileStore {
public:
    explicit ProfileStore(QString directory);
    QJsonObject snapshot() const { return document; }
    bool update(const QJsonObject &fields);
    QString error() const { return errorText; }
    bool readable() const { return valid; }
    QString directory() const { return storage; }

private:
    QString storage, errorText;
    QJsonObject document{{"version", 1}, {"cards", QJsonArray{}}};
    bool valid = true;
};
