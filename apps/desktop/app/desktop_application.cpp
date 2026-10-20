#include "desktop_application.h"
#include <QFileOpenEvent>

bool DesktopApplication::event(QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        const auto *file = static_cast<QFileOpenEvent *>(event);
        if (file->url().isLocalFile())
            emit fileOpened(file->url());
        return true;
    }

    return QGuiApplication::event(event);
}
