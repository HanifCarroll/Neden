#include "credentials.h"

QByteArray readSavedCredential(const QString &) {
    // Fail closed until an OS credential-store adapter has been verified.
    return {};
}

QString authorizeSavedCredential(const QString &) {
    return "This platform needs a verified credential-store adapter.";
}

QByteArray readSavedSubtitleLogin() {
    return {};
}

QString saveCredential(const QString &, QByteArray key) {
    key.fill(0);
    return "This platform needs a verified credential-store adapter. Nothing was saved.";
}
