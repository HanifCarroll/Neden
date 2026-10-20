#include "subtitle_service.h"
#include "network_error.h"
#include <QSslSocket>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <QStringDecoder>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QtEndian>
#include <stdexcept>

namespace {
constexpr qsizetype maximumResponse = 8 * 1024 * 1024;
}
SubtitleService::SubtitleService(QString directory, AppSettings &preferences,
                                 AiClient::CredentialReader credentialReader, QObject *parent,
                                 QString endpoint)
    : QObject(parent), storage(std::move(directory)), subtitleEndpoint(std::move(endpoint)),
      settings(preferences), readCredential(std::move(credentialReader)) {
    if (!AiClient::allowedEndpoint(QUrl(subtitleEndpoint)))
        throw std::invalid_argument("Invalid subtitle API endpoint.");
}
void SubtitleService::setMedia(const QString &path, bool search) {
    cancel();
    results.clear();
    mediaPath = path;
    if (settings.automaticSubtitles() && search && !path.isEmpty())
        findSubtitles("tr", true);
    emit changed();
}

QNetworkRequest SubtitleService::authenticatedRequest(const QUrl &url) {
    if (url.scheme() == "https" && !QSslSocket::supportsSsl()) {
        throw std::runtime_error(
            "This installation is missing secure networking support. Reinstall Neden; no request was sent.");
    }

    auto key = readCredential(subtitleEndpoint);
    if (key.isEmpty() || key.size() > 512 || key.contains('\r') || key.contains('\n')) {
        throw std::runtime_error(
            "The saved credential is missing or Keychain access is unavailable. No request was sent.");
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(60000);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "Neden v0.1");
    request.setRawHeader("Api-Key", key);
    if (!subtitleToken.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + subtitleToken);
    key.fill(0);
    return request;
}

QString SubtitleService::movieHash(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 131072)
        return {};
    quint64 hash = file.size();
    for (qint64 offset : {qint64{0}, file.size() - 65536}) {
        if (!file.seek(offset))
            return {};
        const auto bytes = file.read(65536);
        if (bytes.size() != 65536)
            return {};
        for (int index = 0; index < bytes.size(); index += 8) {
            hash += qFromLittleEndian<quint64>(bytes.constData() + index);
        }
    }
    return QString::number(hash, 16).rightJustified(16, '0');
}

void SubtitleService::cancel() {
    requestGeneration++;
    if (!subtitleReply)
        return;
    subtitleReply->disconnect(this);
    subtitleReply->abort();
    subtitleReply->deleteLater();
    subtitleReply = nullptr;
}

void SubtitleService::findSubtitles(const QString &language, bool autoDownload, const QString &title,
                                    int season, int episode) {

    // 1. Cancel the previous lookup and fingerprint the current media.
    cancel();
    results.clear();
    if (mediaPath.isEmpty() || (language != "tr" && language != "en") || title.size() > 200 || season < 0 ||
        season > 1000 || episode < 0 || episode > 10000 ||
        (title.trimmed().isEmpty() && (season || episode))) {
        setMessage("Open a local video and choose Turkish or English subtitles.");
        return;
    }
    try {
        QUrl url(subtitleEndpoint + "/subtitles");
        QUrlQuery query;
        if (episode)
            query.addQueryItem("episode_number", QString::number(episode));
        query.addQueryItem("languages", language);
        if (title.trimmed().isEmpty() || autoDownload) {
            const auto hash = movieHash(mediaPath);
            if (hash.isEmpty()) {
                setMessage("This file cannot be fingerprinted (128 KiB minimum).");
                return;
            }
            query.addQueryItem("moviehash", hash);
        } else {
            query.addQueryItem("query", title.trimmed());
            if (season)
                query.addQueryItem("season_number", QString::number(season));
            if (season || episode)
                query.addQueryItem("type", "episode");
        }
        url.setQuery(query);

        // 2. Keep this bounded response owned by the current media request.
        auto request = authenticatedRequest(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::UserVerifiedRedirectPolicy);
        request.setMaximumRedirectsAllowed(2);
        auto *reply = networkAccessManager.get(request);
        connect(reply, &QNetworkReply::redirected, this, [reply, url](const QUrl &redirect) {
            const auto destination = reply->url().resolved(redirect);
            if (destination.scheme() == url.scheme() && destination.host() == url.host() &&
                destination.port() == url.port() && destination.path() == url.path() &&
                destination.userInfo().isEmpty()) {
                reply->redirectAllowed();
            } else {
                reply->abort();
            }
        });
        subtitleReply = reply;
        const auto requestGenerationAtStart = requestGeneration;
        connect(reply, &QNetworkReply::readyRead, this, [reply] {
            if (reply->bytesAvailable() > maximumResponse)
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, requestGenerationAtStart, autoDownload] {
            reply->deleteLater();
            if (requestGenerationAtStart != requestGeneration)
                return;
            subtitleReply = nullptr;
            if (reply->error() != QNetworkReply::NoError ||
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                setMessage(
                    networkFailureMessage(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                                          reply->error(), "Subtitle search failed"));
                return;
            }

            // 3. Offer valid file IDs; automatic downloads require an exact hash match.
            const auto data = QJsonDocument::fromJson(reply->readAll()).object().value("data").toArray();
            for (const auto &item : data) {
                const auto attributes = item.toObject().value("attributes").toObject();
                for (const auto &file : attributes.value("files").toArray()) {
                    if (results.size() >= 50)
                        break;
                    const auto fileId = file.toObject().value("file_id").toInt();
                    if (fileId <= 0)
                        continue;
                    results.append(QVariantMap{{"fileId", fileId},
                                               {"release", attributes.value("release").toString()},
                                               {"language", attributes.value("language").toString()},
                                               {"exact", attributes.value("moviehash_match").toBool()}});
                }
            }
            setMessage(results.isEmpty()
                           ? "No matching subtitles found."
                           : "Choose a subtitle release. Downloads use your OpenSubtitles allowance.");
            if (autoDownload && !results.isEmpty() && results.first().toMap().value("exact").toBool())
                downloadSubtitle(0);
        });
        setMessage(title.trimmed().isEmpty() || autoDownload
                       ? "Searching OpenSubtitles by video fingerprint…"
                       : "Searching OpenSubtitles by title and episode…");
    } catch (const std::exception &error) {
        setMessage(QString::fromUtf8(error.what()));
    }
}

void SubtitleService::downloadSubtitle(int index) {
    if (index < 0 || index >= results.size())
        return;
    cancel();
    const auto requestGenerationAtStart = requestGeneration;
    if (!subtitleToken.isEmpty() || !subtitleLoginReader) {
        requestSubtitleDownload(index, requestGenerationAtStart);
        return;
    }

    // 1. Login once for this app session using private Keychain account data.
    try {
        auto login = subtitleLoginReader();
        const auto account = QJsonDocument::fromJson(login).object();
        if (account.value("username").toString().isEmpty() ||
            account.value("password").toString().isEmpty()) {
            setMessage("Connect the OpenSubtitles Keychain account in Settings before downloading.");
            return;
        }
        const auto request = authenticatedRequest(QUrl(subtitleEndpoint + "/login"));
        auto *reply = networkAccessManager.post(request, login);
        login.fill(0);
        subtitleReply = reply;
        connect(reply, &QNetworkReply::readyRead, this, [reply] {
            if (reply->bytesAvailable() > maximumResponse)
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, requestGenerationAtStart, index] {
            reply->deleteLater();
            if (requestGenerationAtStart != requestGeneration)
                return;
            subtitleReply = nullptr;
            const auto data = QJsonDocument::fromJson(reply->readAll()).object();
            const auto token = data.value("token").toString().toUtf8();
            const auto base = data.value("base_url").toString();
            if (reply->error() != QNetworkReply::NoError ||
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
                token.isEmpty() || token.size() > 8192 || token.contains('\r') || token.contains('\n') ||
                (!base.isEmpty() && base != QUrl(subtitleEndpoint).host())) {
                setMessage(
                    reply->error() != QNetworkReply::NoError
                        ? networkFailureMessage(
                              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                              reply->error(), "OpenSubtitles sign-in failed")
                        : QString(
                              "OpenSubtitles returned an unsupported sign-in response. No retry was made."));
                return;
            }
            subtitleToken = token;

            // 2. Continue the original download only while its media still owns it.
            requestSubtitleDownload(index, requestGenerationAtStart);
        });
        setMessage("Signing in to OpenSubtitles…");
    } catch (const std::exception &error) {
        setMessage(QString::fromUtf8(error.what()));
    }
}

void SubtitleService::requestSubtitleDownload(int index, quint64 requestGenerationAtStart) {
    if (requestGenerationAtStart != requestGeneration || index < 0 || index >= results.size())
        return;
    try {
        const auto request = authenticatedRequest(QUrl(subtitleEndpoint + "/download"));
        const QJsonObject body{{"file_id", results[index].toMap().value("fileId").toInt()},
                               {"sub_format", "srt"}};
        auto *reply = networkAccessManager.post(request, QJsonDocument(body).toJson());
        subtitleReply = reply;
        connect(reply, &QNetworkReply::readyRead, this, [reply] {
            if (reply->bytesAvailable() > maximumResponse)
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, requestGenerationAtStart] {
            reply->deleteLater();
            if (requestGenerationAtStart != requestGeneration)
                return;
            subtitleReply = nullptr;
            if (reply->error() != QNetworkReply::NoError ||
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401)
                    subtitleToken.clear();
                setMessage(
                    networkFailureMessage(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                                          reply->error(), "Subtitle download failed"));
                return;
            }
            const auto link =
                QUrl(QJsonDocument::fromJson(reply->readAll()).object().value("link").toString());
            const bool localTest = QUrl(subtitleEndpoint).scheme() == "http" && link.scheme() == "http" &&
                                   link.host() == QUrl(subtitleEndpoint).host() &&
                                   link.port() == QUrl(subtitleEndpoint).port();
            if ((!localTest && link.scheme() != "https") || link.host().isEmpty() ||
                !link.userInfo().isEmpty()) {
                setMessage("OpenSubtitles returned an invalid download link.");
                return;
            }
            fetchSubtitle(link, requestGenerationAtStart);
        });
        setMessage("Downloading selected subtitle…");
    } catch (const std::exception &error) {
        setMessage(QString::fromUtf8(error.what()));
    }
}

void SubtitleService::fetchSubtitle(const QUrl &url, quint64 requestGenerationAtStart) {

    // 1. Fetch the signed file URL without an API key or authorization header.
    QNetworkRequest request(url);
    request.setTransferTimeout(60000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = networkAccessManager.get(request);
    subtitleReply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->bytesAvailable() > maximumResponse)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestGenerationAtStart] {
        reply->deleteLater();
        if (requestGenerationAtStart != requestGeneration)
            return;
        subtitleReply = nullptr;
        if (reply->error() != QNetworkReply::NoError ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            setMessage(
                networkFailureMessage(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                                      reply->error(), "Subtitle file download failed"));
            return;
        }
        try {

            // 2. Validate the complete subtitle file before storing it for future resumes.
            QStringDecoder decoder(QStringDecoder::Utf8);
            const QByteArray bytes = reply->readAll();
            const QString text = decoder.decode(bytes);
            if (decoder.hasError())
                throw std::runtime_error("Invalid UTF-8 subtitles.");
            const auto cues = parseSrt(text);

            // 3. Commit an owner-readable local copy before publishing its stable path.
            const QString directory = storage + "/subtitles";
            const QString filename =
                QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) +
                ".srt";
            const QString path = directory + "/" + filename;
            QSaveFile file(path);
            if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) ||
                !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
                file.write(bytes) != bytes.size() || !file.commit()) {
                setMessage(
                    "Downloaded subtitles could not be saved. Check free space and folder permissions.");
                return;
            }

            emit subtitlesReady(cues, path);
            setMessage("Downloaded subtitles loaded. Check their timing against the dialogue.");
        } catch (...) {
            setMessage("The download is not a supported SRT subtitle file.");
        }
    });
}
