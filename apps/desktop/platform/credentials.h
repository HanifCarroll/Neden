#pragma once
#include <QByteArray>
#include <QString>

// Privileged service code only. Never expose the result as a QML property.
QByteArray readSavedCredential(const QString &endpoint);

// Explicit user action: returns status only, never secret bytes.
QString authorizeSavedCredential(const QString &endpoint);
QByteArray readSavedSubtitleLogin();

// Save only from an explicit settings action; return status, never the saved key.
QString saveCredential(const QString &endpoint, QByteArray key);
