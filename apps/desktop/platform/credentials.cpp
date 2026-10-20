#include "credentials.h"
#include <qtkeychain/keychain.h>

namespace {
QString accessError(QKeychain::Error error) {
    switch (error) {
    case QKeychain::EntryNotFound:
        return "No saved Neden credential exists for this endpoint/account.";
    case QKeychain::AccessDenied:
    case QKeychain::AccessDeniedByUser:
        return "Credential access was cancelled or denied. No request was sent.";
    case QKeychain::NoBackendAvailable:
    case QKeychain::NotImplemented:
        return "A system credential store is unavailable. Nothing was saved to preferences.";
    default:
        return "The system credential store could not complete the operation. Please try again.";
    }
}
}
QString CredentialStore::serviceName(const QString &endpoint) {
    // Retain the IINA namespace and account names: existing keys need no migration.
    return "io.github.hanifcarroll.iina-language-learning - api:" + endpoint;
}
bool CredentialStore::validSecret(const QByteArray &secret, const QString &account) {
    return !secret.isEmpty() && secret.size() <= (account == "login" ? 8192 : 512)
        && !secret.contains('\r') && !secret.contains('\n');
}
QByteArray CredentialStore::read(const QString &endpoint) {
    if (authorizedKeys.contains(endpoint))
        return authorizedKeys.value(endpoint);
    auto key = readCredentialWithoutInteraction(serviceName(endpoint), "api-key");
    if (validSecret(key, "api-key")) {
        authorizedKeys.insert(endpoint, key);
        return key;
    }
    key.fill(0);
    return {};
}
QByteArray CredentialStore::subtitleLogin() const {
    return authorizedKeys.value("opensubtitles-login");
}
void CredentialStore::authorize(const QString &endpoint, Completion completed) {
    if (operationPending) {
        completed("Finish the current credential operation first.");
        return;
    }
    operationPending = true;
    const QStringList accounts = endpoint == "https://api.opensubtitles.com/api/v1"
        ? QStringList{"api-key", "login"} : QStringList{"api-key"};
    readAccounts(endpoint, accounts, 0, {}, std::move(completed));
}
void CredentialStore::readAccounts(const QString &endpoint, const QStringList &accounts, int index,
                                  QHash<QString, QByteArray> values, Completion completed) {
    if (index == accounts.size()) {
        // Publish only a complete successful authorization, including subtitle login.
        for (auto entry = values.cbegin(); entry != values.cend(); ++entry)
            authorizedKeys.insert(entry.key() == "login" ? "opensubtitles-login" : endpoint, entry.value());
        operationPending = false;
        completed("Connected. The saved credentials are available for this app session.");
        return;
    }
    auto *job = new QKeychain::ReadPasswordJob(serviceName(endpoint), this);
    job->setKey(accounts.at(index));
    job->setInsecureFallback(false);
    connect(job, &QKeychain::Job::finished, this,
        [this, job, endpoint, accounts, index, values = std::move(values), completed = std::move(completed)]() mutable {
            auto key = job->binaryData();
            if (job->error() != QKeychain::NoError || !validSecret(key, accounts.at(index))) {
                key.fill(0);
                operationPending = false;
                completed(job->error() == QKeychain::NoError ? "The saved credential has an invalid format." : accessError(job->error()));
                return;
            }
            values.insert(accounts.at(index), key);
            readAccounts(endpoint, accounts, index + 1, std::move(values), std::move(completed));
        });
    job->start();
}
void CredentialStore::save(const QString &endpoint, QByteArray key, Completion completed) {
    if (!validSecret(key, "api-key")) {
        key.fill(0);
        completed("Use a nonempty API key of at most 512 bytes without line breaks.");
        return;
    }
    if (operationPending) {
        key.fill(0);
        completed("Finish the current credential operation first.");
        return;
    }
    operationPending = true;
    auto *job = new QKeychain::WritePasswordJob(serviceName(endpoint), this);
    job->setKey("api-key");
    job->setInsecureFallback(false);
    job->setBinaryData(key);
    connect(job, &QKeychain::Job::finished, this,
        [this, job, endpoint, key, completed = std::move(completed)]() mutable {
            operationPending = false;
            if (job->error() == QKeychain::NoError)
                authorizedKeys.insert(endpoint, key);
            key.fill(0);
            completed(job->error() == QKeychain::NoError ? "Key saved securely in the system credential store." : accessError(job->error()));
        });
    key.fill(0);
    job->start();
}
