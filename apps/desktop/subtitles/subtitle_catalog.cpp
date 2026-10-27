#include "subtitle_catalog.h"
#include "ai_client.h"
#include "network_error.h"
#include "language_catalog.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>
#include <stdexcept>

namespace {
constexpr qsizetype responseLimit = 8 * 1024 * 1024;
bool sameOrigin(const QUrl &left, const QUrl &right) {
    return left.scheme() == right.scheme() && left.host() == right.host() && left.port() == right.port()
        && right.userInfo().isEmpty();
}
bool validIdentifier(const QString &value) {
    static const QRegularExpression identifier("^[A-Za-z0-9_-]{1,120}$");
    return identifier.match(value).hasMatch();
}
QString jsonIdentifier(const QJsonValue &value) {
    return value.isString() ? value.toString() : value.isDouble() ? QString::number(value.toInteger()) : QString();
}
}
SubtitleCatalog::SubtitleCatalog(CredentialReader reader, QObject *parent, QString vault,
                                 QString dl, QString downloads)
    : QObject(parent), credentials(std::move(reader)), vaultOrigin(std::move(vault)),
      dlOrigin(std::move(dl)), downloadOrigin(std::move(downloads)) {
    for (const auto &origin : {vaultOrigin, dlOrigin, downloadOrigin}) {
        if (!AiClient::allowedEndpoint(QUrl(origin)))
            throw std::invalid_argument("Invalid subtitle catalog origin");
    }
}
void SubtitleCatalog::cancel() {
    ++generation;
    if (reply) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }
}
void SubtitleCatalog::get(const QUrl &url, ResponseHandler completed, bool subdlCredential) {
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("User-Agent", "Neden/0.1");
    if (subdlCredential) {
        if (!sameOrigin(QUrl(dlOrigin), url) && !sameOrigin(QUrl(downloadOrigin), url)) {
            emit messageChanged("SubDL returned an unsupported download address.");
            return;
        }
        auto key = credentials(dlOrigin);
        if (key.isEmpty() || key.size() > 512 || key.contains('\r') || key.contains('\n')) {
            emit messageChanged("Connect your SubDL key in Settings → AI & accounts → Subtitle downloads.");
            return;
        }
        request.setRawHeader("X-API-Key", key);
        key.fill(0);
    }
    auto *pending = network.get(request);
    pending->setReadBufferSize(responseLimit + 1);
    reply = pending;
    const auto started = generation;
    connect(pending, &QNetworkReply::readyRead, this, [pending] {
        if (pending->bytesAvailable() > responseLimit)
            pending->abort();
    });
    connect(pending, &QNetworkReply::finished, this, [this, pending, started, completed = std::move(completed)] {
        pending->deleteLater();
        if (started != generation)
            return;
        reply = nullptr;
        const auto status = pending->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 423) {
            emit messageChanged("This catalog is refreshing the show. Try again shortly.");
            return;
        }
        if (status == 429) {
            emit messageChanged("This source's allowance or rate limit was reached. Try another source or retry later.");
            return;
        }
        if (pending->error() != QNetworkReply::NoError || status != 200 || pending->bytesAvailable() > responseLimit) {
            emit messageChanged(networkFailureMessage(status, pending->error(), "Subtitle request failed"));
            return;
        }
        completed(pending->readAll());
    });
}
void SubtitleCatalog::search(const QString &source, const QString &title, const QString &code,
                            int seasonNumber, int episodeNumber) {
    cancel();
    provider = source;
    language = LanguageCatalog::subtitleLocaleCode(code);
    // Search pickers retain OpenSubtitles IDs for saved preferences. Convert at
    // this boundary; neither source uses aliases such as az-az, ma or pt-pt.
    if (provider == "subdl" && language == "zh") language = "zh-cn";
    if (provider == "subdl" && language == "fil") language = "tl";
    season = seasonNumber;
    episode = episodeNumber;
    if ((provider != "subvault" && provider != "subdl") || title.trimmed().size() < 3 || title.size() > 200) {
        emit messageChanged("Enter at least three characters of the title.");
        return;
    }
    if (language.isEmpty() || season < 0 || season > 1000 || episode < 0 || episode > 10000) {
        emit messageChanged("Choose a subtitle language and valid season/episode numbers.");
        return;
    }
    if (provider == "subvault" && episode < 1) {
        emit messageChanged("Subvault searches TV episodes. Choose a season and episode.");
        return;
    }
    QUrl url;
    if (provider == "subvault") {
        url = QUrl(vaultOrigin + "/shows/search/" + QString::fromLatin1(QUrl::toPercentEncoding(title.trimmed())));
    } else {
        url = QUrl(dlOrigin + "/api/v2/movies/search");
        QUrlQuery query;
        query.addQueryItem("q", title.trimmed());
        query.addQueryItem("limit", "20");
        if (episode > 0) query.addQueryItem("type", "tv");
        url.setQuery(query);
    }
    emit messageChanged("Finding matching titles…");
    get(url, [this](const QByteArray &bytes) {
        const auto document = QJsonDocument::fromJson(bytes);
        if (!document.isObject()) { emit messageChanged("The source returned an invalid search response."); return; }
        const auto items = document.object().value(provider == "subvault" ? "shows" : "results").toArray();
        QVariantList titles;
        for (const auto &item : items) {
            const auto value = item.toObject();
            const auto id = jsonIdentifier(value.value(provider == "subvault" ? "id" : "sd_id"));
            const auto name = value.value("name").toString().left(300);
            if (!validIdentifier(id) || name.isEmpty() || titles.size() >= 20) continue;
            titles.append(QVariantMap{{"provider", provider}, {"kind", "title"}, {"id", id},
                {"release", name}, {"language", language}, {"mediaType", value.value("type").toString()},
                {"year", value.value("year").toInt()}});
        }
        emit resultsReady(titles, titles.isEmpty() ? "No matching titles found. Try another source or title."
                                                  : "Choose the matching title to see subtitle releases.");
    }, provider == "subdl");
}
void SubtitleCatalog::searchTitle(const QVariantMap &title) {
    const auto id = title.value("id").toString();
    if (!validIdentifier(id)) return;
    QUrl url;
    if (provider == "subvault") {
        url = QUrl(vaultOrigin + "/subtitles/get/" + id + "/" + QString::number(season)
                   + "/" + QString::number(episode) + "/" + language);
    } else {
        url = QUrl(dlOrigin + "/api/v2/subtitles/search");
        QUrlQuery query;
        query.addQueryItem("sd_id", id);
        query.addQueryItem("languages", language);
        query.addQueryItem("unpack", "1");
        query.addQueryItem("subs_per_page", "30");
        if (title.value("mediaType").toString() == "tv" || episode > 0) {
            if (episode < 1) { emit messageChanged("Choose an episode for this TV show, then search again."); return; }
            query.addQueryItem("type", "tv");
            query.addQueryItem("season", QString::number(season));
            query.addQueryItem("episode", QString::number(episode));
        }
        url.setQuery(query);
    }
    emit resultsReady({}, "Finding subtitle releases…");
    get(url, [this](const QByteArray &bytes) { publishSubtitles(bytes); }, provider == "subdl");
}
QUrl SubtitleCatalog::downloadUrl(const QString &path) const {
    const QUrl origin(provider == "subvault" ? vaultOrigin : downloadOrigin);
    auto url = origin.resolved(QUrl(path));
    // Keys supplied in SubDL's response URLs must never reach models, logs or URLs.
    url.setQuery(QString());
    url.setFragment(QString());
    if (!sameOrigin(origin, url) || !url.path().startsWith(provider == "subvault" ? "/subtitles/download/" : "/subtitle/")
        || url.path().contains("..") || url.path().endsWith(".zip", Qt::CaseInsensitive))
        return {};
    return url;
}
void SubtitleCatalog::publishSubtitles(const QByteArray &bytes) {
    const auto document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) { emit messageChanged("The source returned invalid subtitle results."); return; }
    const auto items = document.object().value(provider == "subvault" ? "matchingSubtitles" : "subtitles").toArray();
    QVariantList releases;
    auto append = [&](const QJsonObject &entry, const QString &path, const QString &name) {
        const auto url = downloadUrl(path);
        if (url.isEmpty() || name.isEmpty() || releases.size() >= 50) return;
        releases.append(QVariantMap{{"kind", "subtitle"}, {"provider", provider}, {"release", name.left(400)},
                                   {"language", language}, {"download", url.toString()},
                                   {"hearingImpaired", entry.value("hi").toBool(entry.value("hearingImpaired").toBool())}});
    };
    for (const auto &item : items) {
        const auto entry = item.toObject();
        if (provider == "subvault") {
            if (!entry.value("completed").toBool()) continue;
            append(entry, entry.value("downloadUri").toString(), entry.value("release").toString(entry.value("version").toString()));
        } else {
            const auto files = entry.value("unpack_files").toArray();
            for (const auto &file : files) {
                const auto value = file.toObject();
                if (episode > 0 && (value.value("season").toInt(-1) != season || value.value("episode").toInt(-1) != episode)) continue;
                append(value, value.value("url").toString(), value.value("release_name").toString(value.value("name").toString()));
            }
            if (files.isEmpty() && !entry.value("full_season").toBool())
                append(entry, entry.value("url").toString(), entry.value("release_name").toString());
        }
    }
    emit resultsReady(releases, releases.isEmpty() ? "No individual subtitle releases found for this title and language. Try another source."
                                                 : "Choose a release. Check its timing after loading.");
}
void SubtitleCatalog::select(const QVariantMap &result) {
    cancel();
    if (result.value("provider").toString() != provider) return;
    if (result.value("kind").toString() == "title") { searchTitle(result); return; }
    const auto url = downloadUrl(result.value("download").toString());
    if (url.isEmpty()) { emit messageChanged("This source did not provide an individual subtitle file."); return; }
    emit messageChanged("Downloading selected subtitle…");
    get(url, [this](QByteArray bytes) { emit fileReady(std::move(bytes)); }, provider == "subdl");
}
