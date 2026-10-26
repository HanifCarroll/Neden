// Opt-in staging check. No GUI, API keys, passwords or tokens are printed or persisted.
#include "study_services.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    if (argc != 2) return 2;
    QTemporaryDir profile;
    StudyServices study(profile.path(), [](const QString &) { return QByteArray(); });
    auto *account = study.account();
    QByteArray saved;
    account->setCredentialStore([&](auto done) { done(saved, {}); },
                                [&](QByteArray token, auto done) { saved = token; done({}); });
    QObject::connect(account, &HostedAccount::browserRequested, account, [&](const QUrl &url) {
        QFile output(QString::fromLocal8Bit(argv[1]));
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) { application.exit(2); return; }
        output.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        output.write(url.toEncoded());
        QTextStream(stdout) << "Authorization link ready.\n";
    });
    const QJsonObject context{{"selection", "Eyvallah."}, {"clickedSubtitle", "Eyvallah."},
                              {"sourceLanguage", "Turkish"}, {"explanationLanguage", "English"}};
    int phase = 0;
    const auto finish = [&](bool success) {
        phase = 5;
        account->signOut();
        QTextStream(stdout) << "Native hosted integration: " << (success ? "PASS" : "FAIL") << "\n";
        QTimer::singleShot(1500, &application, [&, success] { application.exit(success ? 0 : 1); });
    };
    QObject::connect(account, &HostedAccount::changed, account, [&] {
        if (!account->connected() || account->busy()) return;
        if (phase == 0) {
            phase = 1;
            QTextStream(stdout) << "Browser authorization: PASS\n";
            study.words()->lookupContext("Eyvallah", context, {}, {});
        } else if (phase == 2) {
            phase = 3;
            QTextStream(stdout) << "Saved-token refresh: PASS\n";
            study.explanation()->explainContext(context);
        }
    });
    QObject::connect(study.words(), &WordLookup::changed, account, [&] {
        if (phase != 1) return;
        if (study.words()->state() == "failed") { finish(false); return; }
        if (study.words()->state() != "complete") return;
        QTextStream(stdout) << "Word card parsing through hosted transport: PASS\n";
        phase = 2;
        account->connectSaved();
    });
    QObject::connect(study.explanation(), &ExplanationSession::changed, account, [&] {
        if (phase != 3 && phase != 4) return;
        const auto state = study.explanation()->state();
        if (state == "failed" || state == "incomplete") { finish(false); return; }
        if (state != "complete") return;
        if (phase == 3) {
            phase = 4;
            QTextStream(stdout) << "Streaming explanation: PASS\n";
            if (!study.explanation()->followUp("Can I use this to thank a friend?")) finish(false);
        } else {
            QTextStream(stdout) << "Follow-up with preserved context: PASS\n";
            finish(true);
        }
    });
    QTimer::singleShot(240000, &application, [&] { application.exit(3); });
    account->signIn();
    return application.exec();
}
