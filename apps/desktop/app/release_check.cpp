#include "release_check.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QVersionNumber>

ReleaseCheck::ReleaseCheck(QObject *parent) : QObject(parent) {}
QString ReleaseCheck::currentVersion() const { return QStringLiteral(NEDEN_VERSION); }

QUrl ReleaseCheck::newerRelease(const QJsonObject &release, const QString &installedVersion) {
    if (release.value("draft").toBool() || release.value("prerelease").toBool())
        return {};
    const auto tag = release.value("tag_name").toString();
    static const QRegularExpression stableTag(QStringLiteral("^v?([0-9]+\\.[0-9]+\\.[0-9]+)$"));
    const auto match = stableTag.match(tag);
    if (!match.hasMatch() || QVersionNumber::fromString(match.captured(1)) <= QVersionNumber::fromString(installedVersion))
        return {};
    const QUrl expected(QStringLiteral("https://github.com/HanifCarroll/Neden/releases/tag/") + tag);
    return QUrl(release.value("html_url").toString()) == expected ? expected : QUrl{};
}

void ReleaseCheck::check() {
    if (inProgress)
        return;
    inProgress = true;
    availableRelease = QUrl{};
    statusMessage = QStringLiteral("Checking for a stable release…");
    emit changed();
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/HanifCarroll/Neden/releases/latest")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "Neden-Update-Check");
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    auto *reply = network.get(request);
    reply->setReadBufferSize(256 * 1024 + 1);
    connect(reply, &QIODevice::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 256 * 1024)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        inProgress = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404) {
            statusMessage = QStringLiteral("No stable release has been published yet. Preview downloads are on the releases page.");
        } else if (reply->error() != QNetworkReply::NoError) {
            statusMessage = QStringLiteral("Couldn’t check for updates. Try again later or visit the releases page.");
        } else {
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(reply->readAll(), &error);
            const auto release = document.object();
            if (error.error != QJsonParseError::NoError || !document.isObject() || !release.contains("tag_name")) {
                statusMessage = QStringLiteral("The release service returned an unexpected response. Please try again later.");
            } else {
                availableRelease = newerRelease(release, currentVersion());
                statusMessage = availableRelease.isEmpty()
                    ? QStringLiteral("There is no newer stable release available.")
                    : QStringLiteral("%1 is available. Read the release notes before installing.").arg(release.value("tag_name").toString());
            }
        }
        reply->deleteLater();
        emit changed();
    });
}
