#include "hosted_credentials.h"
#include "hosted_account.h"
#include "credentials.h"
#include <qtkeychain/keychain.h>
#include <QPointer>

void configureHostedCredentials(HostedAccount &account) {
    const QString service = account.credentialService();
    QPointer<HostedAccount> owner(&account);
    account.setCredentialStore(
        [owner, service](HostedAccount::ReadCompletion completed) {
            if (!owner) return;
            auto *job = new QKeychain::ReadPasswordJob(service, owner);
            job->setKey("refresh-token");
            job->setInsecureFallback(false);
            QObject::connect(job, &QKeychain::Job::finished, owner, [job, completed = std::move(completed)] {
                completed(job->error() == QKeychain::NoError ? job->binaryData() : QByteArray(),
                          job->error() == QKeychain::NoError ? QString() : "Saved sign-in is unavailable.");
            });
            job->start();
        },
        [owner, service](QByteArray token, HostedAccount::WriteCompletion completed) {
            if (!owner) return;
            QKeychain::Job *job;
            if (token.isEmpty()) {
                auto *remove = new QKeychain::DeletePasswordJob(service, owner);
                remove->setKey("refresh-token");
                job = remove;
            } else {
                auto *write = new QKeychain::WritePasswordJob(service, owner);
                write->setKey("refresh-token");
                write->setBinaryData(token);
                token.fill(0);
                job = write;
            }
            job->setInsecureFallback(false);
            QObject::connect(job, &QKeychain::Job::finished, owner, [job, completed = std::move(completed)] {
                completed(job->error() == QKeychain::NoError || job->error() == QKeychain::EntryNotFound
                              ? QString() : "The system credential store is unavailable.");
            });
            job->start();
        });
}

void restoreHostedCredentialsWithoutInteraction(HostedAccount &account) {
    account.restoreToken(readCredentialWithoutInteraction(account.credentialService(), "refresh-token"));
}
