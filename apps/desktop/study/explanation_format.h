#pragma once
#include <QObject>
#include <QColor>
#include <QtQml/qqmlregistration.h>

class ExplanationFormat : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    using QObject::QObject;
    Q_INVOKABLE static QString render(const QString &markdown, const QColor &linkColor = QColor("#e6eaf2"));
    Q_INVOKABLE static QString breakdown(const QString &markdown, const QString &link);
};
