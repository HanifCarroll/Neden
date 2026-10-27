// Opt-in checks against real subtitle files and an explicitly supplied AI key.
// Credentials come only from the environment and are never printed.
#include "subtitle_translation.h"
#include "subtitle_timeline.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTimer>
#include <QTextStream>
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() < 2 || arguments.size() > 3)
        return 2;
    try {
        const auto source = SubtitleTimeline::read(QUrl::fromLocalFile(arguments[1]));
        QTextStream(stdout) << "Parsed " << source.size() << " subtitle lines.\n";
        if (arguments.size() == 2)
            return 0;
        if (qEnvironmentVariableIsEmpty("NEDEN_TRANSLATION_CHECK_KEY"))
            return 2;
        QTemporaryDir directory;
        ProfileStore profile(directory.path());
        AppSettings settings(profile);
        SubtitleTranslation translation(
            settings, [](const QString &) { return qgetenv("NEDEN_TRANSLATION_CHECK_KEY"); });
        QObject::connect(&translation, &SubtitleTranslation::failed, &application,
                         [&](const QString &message) {
                             QTextStream(stderr) << message << '\n';
                             application.exit(1);
                         });
        QObject::connect(
            &translation, &SubtitleTranslation::ready, &application, [&](const QVector<Cue> &result) {
                if (result.size() != source.size()) {
                    application.exit(1);
                    return;
                }
                for (int i = 0; i < result.size(); ++i) {
                    if (result[i].startMs != source[i].startMs || result[i].endMs != source[i].endMs) {
                        application.exit(1);
                        return;
                    }
                    QTextStream(stdout) << result[i].text << '\n';
                }
                QTextStream(stdout) << "Every line translated; every timestamp preserved.\n";
                application.exit(0);
            });
        QTimer::singleShot(120000, &application, [&] {
            translation.cancel();
            application.exit(1);
        });
        QTimer::singleShot(0, &application, [&] { translation.start(source, arguments[2]); });
        return application.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << '\n';
        return 1;
    }
}
