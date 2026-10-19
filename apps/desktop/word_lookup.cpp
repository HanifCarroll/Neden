#include "word_lookup.h"
#include "network_error.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>
#include <stdexcept>

WordLookup::WordLookup(CredentialReader reader, QObject *parent, QUrl origin)
    : QObject(parent), credentials(std::move(reader)), testOrigin(std::move(origin)) {
    if (!testOrigin.isEmpty() && !(testOrigin.scheme() == "http" && testOrigin.host() == "127.0.0.1")) {
        throw std::invalid_argument("A word test origin must be loopback HTTP.");
    }
}

QString WordLookup::wordAt(const QString &text, int offset) {
    if (offset < 0 || offset > text.size()) return {};

    static const QRegularExpression words("[\\p{L}\\p{M}\\p{N}]+", QRegularExpression::UseUnicodePropertiesOption);
    auto matches = words.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (offset >= match.capturedStart() && offset <= match.capturedEnd()) return match.captured();
    }
    return {};
}

QString WordLookup::languageCode(const QString &language) {
    static const QHash<QString, QString> names{{"arabic", "ar"}, {"chinese", "zh"}, {"dutch", "nl"},
        {"english", "en"}, {"french", "fr"}, {"german", "de"}, {"greek", "el"}, {"hindi", "hi"},
        {"indonesian", "id"}, {"italian", "it"}, {"japanese", "ja"}, {"korean", "ko"}, {"polish", "pl"},
        {"portuguese", "pt"}, {"russian", "ru"}, {"spanish", "es"}, {"swedish", "sv"}, {"turkish", "tr"},
        {"ukrainian", "uk"}, {"vietnamese", "vi"}};
    const QString normalized = language.trimmed().toLower();
    const QString code = names.value(normalized, normalized);
    if (!QRegularExpression("^[a-z]{2,3}(?:-[a-z]{2,4})?$").match(code).hasMatch()) {
        throw std::runtime_error("Use a supported language name or ISO code in Settings.");
    }
    return code;
}

QStringList WordLookup::parseMeanings(const QByteArray &body, bool google, bool dictionary) {
    if (body.size() > 64000) throw std::runtime_error("The word response exceeded its size limit.");

    const auto document = QJsonDocument::fromJson(body);
    const auto translations = google ? document.object()["data"].toObject()["translations"].toArray()
        : document.array().isEmpty() ? QJsonArray() : document.array().first().toObject()["translations"].toArray();
    QStringList meanings;
    for (const auto &translation : translations) {
        QString meaning = translation.toObject()[google ? "translatedText" : dictionary ? "displayTarget" : "text"].toString().trimmed();
        if (google) {
            meaning.replace("&quot;", "\"").replace("&#39;", "'").replace("&apos;", "'")
                .replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&");
        }
        if (!meaning.isEmpty() && meaning.size() <= 200 && !meanings.contains(meaning)) meanings.append(meaning);
        if (meanings.size() == 5) break;
    }
    return meanings;
}

void WordLookup::cancel() {
    if (reply) {
        auto *previous = reply.data();
        reply = nullptr;
        previous->disconnect(this);
        previous->abort();
        previous->deleteLater();
    }
    selectedWord.clear();
    definitions.clear();
    requestState = "idle";
    statusText.clear();
    emit changed();
}

void WordLookup::lookup(const QString &word, const QVariantMap &settings, bool translationOnly) {
    cancel();
    const auto provider = settings.value("wordProvider").toString();
    if (provider != "google" && provider != "microsoft") return;

    // 1. Validate the single word, languages and private credential before making one explicit request.
    selectedWord = word;
    try {
        if (word.isEmpty() || word.size() > 100 || wordAt(word, 0) != word) throw std::runtime_error("Select one word of at most 100 characters.");

        const auto source = languageCode(settings.value("sourceLanguage", "Turkish").toString());
        const auto target = languageCode(settings.value("explanationLanguage", "English").toString());
        const bool google = provider == "google";
        const bool dictionary = !google && target == "en" && !translationOnly;
        const QString endpoint = google ? "https://translation.googleapis.com" : "https://api.cognitive.microsofttranslator.com";
        auto key = credentials(endpoint);
        if (key.isEmpty() || key.size() > 512 || key.contains('\r') || key.contains('\n')) throw std::runtime_error("Connect or save this provider’s key in Settings.");

        QUrl url((testOrigin.isEmpty() ? endpoint : testOrigin.toString(QUrl::StripTrailingSlash)) + (google ? "/language/translate/v2" : dictionary ? "/dictionary/lookup" : "/translate"));
        if (!google) url.setQuery(QUrlQuery{{"api-version", "3.0"}, {"from", source}, {"to", target}});
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(15000);
        request.setRawHeader(google ? "x-goog-api-key" : "Ocp-Apim-Subscription-Key", key);
        key.fill(0);
        const auto region = settings.value("wordRegion").toString();
        if (!QRegularExpression("^[a-zA-Z0-9-]{0,100}$").match(region).hasMatch()) throw std::runtime_error("Use a valid Microsoft region in Settings.");
        if (!google && !region.isEmpty()) request.setRawHeader("Ocp-Apim-Subscription-Region", region.toUtf8());
        const QByteArray body = google ? QJsonDocument(QJsonObject{{"q", word}, {"source", source}, {"target", target}, {"format", "text"}}).toJson()
            : QJsonDocument(QJsonArray{QJsonObject{{"Text", word}}}).toJson();

        // 2. Bind the bounded response to this lookup; cancellation rejects late callbacks.
        requestState = "loading";
        auto *current = network.post(request, body);
        reply = current;
        current->setReadBufferSize(64001);
        connect(current, &QNetworkReply::readyRead, this, [this, current] {
            if (reply == current && current->bytesAvailable() > 64000) {
                cancel();
                requestState = "failed";
                statusText = "The word response exceeded its size limit.";
                emit changed();
            }
        });
        connect(current, &QNetworkReply::finished, this, [this, current, google, dictionary] {
            if (reply != current) { current->deleteLater(); return; }

            reply = nullptr;
            const int code = current->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (current->error() != QNetworkReply::NoError || code != 200) {
                requestState = "failed";
                statusText = networkFailureMessage(code, current->error(), "Word lookup failed");
            } else {
                try {
                    definitions = parseMeanings(current->readAll(), google, dictionary);
                    requestState = definitions.isEmpty() ? "failed" : "complete";
                    statusText = definitions.isEmpty() ? "No meaning found. Try the dictionary or an AI explanation." : QString();
                } catch (const std::exception &error) {
                    requestState = "failed";
                    statusText = QString::fromUtf8(error.what());
                }
            }
            current->deleteLater();
            emit changed();
        });
        emit changed();
    } catch (const std::exception &error) {
        requestState = "failed";
        statusText = QString::fromUtf8(error.what());
        emit changed();
    }
}
