#include "credentials.h"
#include <qtkeychain/keychain.h>
#include <QUuid>
#include <QtTest>

// Opt-in integration check, run only in disposable CI desktop/keyring sessions.
class CredentialBackendChecks : public QObject {
    Q_OBJECT
    const QString endpoint = "https://credential-test.invalid/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
private slots:
    void saveAndReconnectWithANewStore() {
        CredentialStore writer;
        QString status;
        bool completed = false;
        writer.save(endpoint, "synthetic-integration-key", [&](QString result) { status = result; completed = true; });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 15000);
        QVERIFY2(status.startsWith("Key saved securely"), qPrintable(status));
        CredentialStore reader;
        // Linux/Windows require explicit authorization after a restart.
        QVERIFY(reader.read(endpoint).isEmpty());
        completed = false;
        reader.authorize(endpoint, [&](QString result) { status = result; completed = true; });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 15000);
        QVERIFY2(status.startsWith("Connected."), qPrintable(status));
        QCOMPARE(reader.read(endpoint), QByteArray("synthetic-integration-key"));
    }
    void cleanupTestCase() {
        QKeychain::DeletePasswordJob job(CredentialStore::serviceName(endpoint));
        job.setAutoDelete(false);
        job.setInsecureFallback(false);
        job.setKey("api-key");
        QSignalSpy finished(&job, &QKeychain::Job::finished);
        job.start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 15000);
        QVERIFY(job.error() == QKeychain::NoError || job.error() == QKeychain::EntryNotFound);
    }
};
QTEST_GUILESS_MAIN(CredentialBackendChecks)
#include "credential_backend_checks.moc"
