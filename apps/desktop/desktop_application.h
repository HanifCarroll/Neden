#pragma once
#include <QGuiApplication>
#include <QUrl>

class DesktopApplication : public QGuiApplication {
    Q_OBJECT
public:
    using QGuiApplication::QGuiApplication;
    bool event(QEvent *event) override;
signals:
    void fileOpened(QUrl url);
};
