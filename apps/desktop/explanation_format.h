#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>

class ExplanationFormat : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    using QObject::QObject;
    Q_INVOKABLE static QString render(const QString &markdown);
};
