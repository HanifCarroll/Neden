#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Build a small allowlisted report; never collect logs, paths, URLs, or settings.
class SupportDiagnostics : public QObject {
    Q_OBJECT
    QML_ELEMENT
public:
    using QObject::QObject;
    Q_INVOKABLE QString report(bool hasMedia, bool paused, const QVariantMap &information) const;
    Q_INVOKABLE void copy(bool hasMedia, bool paused, const QVariantMap &information) const;
};
