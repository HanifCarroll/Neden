// Opt-in live catalog check: credentials only from the environment, no GUI or
// subtitle text in the output. Selects one title/release and downloads one file.
#include "subtitle_catalog.h"
#include "cues.h"
#include <QCoreApplication>
#include <QTimer>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() != 6) return 2;
    SubtitleCatalog catalog([](const QString &) { return qgetenv("SUBDL_API_KEY"); });
    QObject::connect(&catalog, &SubtitleCatalog::messageChanged, &application, [](const QString &message) {
        QTextStream(stdout) << message << '\n';
    });
    QObject::connect(&catalog, &SubtitleCatalog::resultsReady, &application,
                     [&](const QVariantList &results, const QString &message) {
        QTextStream(stdout) << message << " (" << results.size() << ")\n";
        if (results.isEmpty()) return;
        const auto selected = results.first().toMap();
        QTimer::singleShot(0, &catalog, [&, selected] { catalog.select(selected); });
    });
    QObject::connect(&catalog, &SubtitleCatalog::fileReady, &application, [&](const QByteArray &bytes) {
        try {
            const auto cues = parseSrt(QString::fromUtf8(bytes));
            QTextStream(stdout) << "Downloaded and parsed " << cues.size() << " cues.\n";
            application.exit(cues.isEmpty() ? 1 : 0);
        } catch (const std::exception &) { application.exit(1); }
    });
    QTimer::singleShot(60000, &application, [&] { catalog.cancel(); application.exit(1); });
    QTimer::singleShot(0, &catalog, [&] {
        catalog.search(arguments[1], arguments[2], arguments[3], arguments[4].toInt(), arguments[5].toInt());
    });
    return application.exec();
}
