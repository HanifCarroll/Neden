#pragma once
#include <QObject>
#include <QWindow>
#include <memory>
class LearningSession;

// Lifetime owns every OS registration and sleep assertion. Construct only in the real application.
class PlatformMediaSession : public QObject {
    Q_OBJECT
public:
    explicit PlatformMediaSession(LearningSession *session, QWindow *window, QObject *parent = nullptr);
    ~PlatformMediaSession() override;

private:
    void update();
    struct Native;
    std::unique_ptr<Native> native;
    LearningSession *session;
    QWindow *window;
};
