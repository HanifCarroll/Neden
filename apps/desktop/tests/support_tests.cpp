#include "release_check.h"
#include "support_diagnostics.h"
#include <QtTest>

class SupportTests : public QObject {
    Q_OBJECT
private slots:
    void acceptsOnlyNewerStableReleasesFromOurRepository() {
        QJsonObject release{{"tag_name", "v0.3.0"}, {"html_url", "https://github.com/HanifCarroll/Neden/releases/tag/v0.3.0"}};
        QVERIFY(!ReleaseCheck::newerRelease(release, "0.2.0").isEmpty());
        QVERIFY(ReleaseCheck::newerRelease(release, "0.3.0").isEmpty());
        QVERIFY(ReleaseCheck::newerRelease(release, "0.10.0").isEmpty());
        release["prerelease"] = true;
        QVERIFY(ReleaseCheck::newerRelease(release, "0.2.0").isEmpty());
        release["prerelease"] = false;
        release["draft"] = true;
        QVERIFY(ReleaseCheck::newerRelease(release, "0.2.0").isEmpty());
        release["draft"] = false;
        release["html_url"] = "https://example.org/update.exe";
        QVERIFY(ReleaseCheck::newerRelease(release, "0.2.0").isEmpty());
        release["tag_name"] = "v0.4.0-preview.1";
        release["html_url"] = "https://github.com/HanifCarroll/Neden/releases/tag/v0.4.0-preview.1";
        QVERIFY(ReleaseCheck::newerRelease(release, "0.2.0").isEmpty());
        QVERIFY(ReleaseCheck::newerRelease({}, "0.2.0").isEmpty());
    }
    void diagnosticsExcludeUntrustedStringsAndIdleMedia() {
        SupportDiagnostics diagnostics;
        const QVariantMap metadata{{"Title", "private-video"}, {"Video codec", "/private/user/token"},
            {"Video", QVariantMap{{"w", 1920}, {"h", 1080}, {"path", "secret"}}},
            {"Audio", QVariantMap{{"samplerate", 48000}}}};
        const auto report = diagnostics.report(true, true, metadata);
        QVERIFY(report.contains("1920 × 1080"));
        QVERIFY(report.contains("48000"));
        QVERIFY(report.contains("paused"));
        QVERIFY(!report.contains("private"));
        QVERIFY(!report.contains("secret"));
        const auto idle = diagnostics.report(false, false, metadata);
        QVERIFY(!idle.contains("1920"));
        QVERIFY(idle.contains("no media"));
    }
};
QTEST_MAIN(SupportTests)
#include "support_tests.moc"
