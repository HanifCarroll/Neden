#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <memory>

// Exposes the platform accessibility preference without changing system settings.
class MotionPreferences : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)
public:
    explicit MotionPreferences(QObject *parent = nullptr);
    ~MotionPreferences() override;
    bool reducedMotion() const;
signals:
    void reducedMotionChanged();
private:
    struct NativeState;
    std::unique_ptr<NativeState> native;
};
