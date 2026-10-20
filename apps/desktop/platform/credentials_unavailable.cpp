#include "credentials.h"

QByteArray readCredentialWithoutInteraction(const QString &, const QString &) {
    // QtKeychain access is asynchronous and explicit on platforms without a silent-read API.
    return {};
}
