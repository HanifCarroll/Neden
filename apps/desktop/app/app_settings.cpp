#include <QPointer>
#include "app_settings.h"
#include "ai_client.h"
#include <stdexcept>
namespace {
const QStringList wordIds{"ai", "off"};
const QStringList primaryIds{"configured", "luna"};
QVariantList choices(const QStringList &ids, const QStringList &names) {
    QVariantList result;
    for (int optionIndex = 0; optionIndex < ids.size(); ++optionIndex)
        result.append(QVariantMap{{"id", ids[optionIndex]}, {"name", names[optionIndex]}});
    return result;
}
} // namespace
QJsonObject LearningSettings::json() const {
    return {{"sourceLanguage", sourceLanguage},
            {"explanationLanguage", explanationLanguage},
            {"includeSecondary", includeSecondary},
            {"noKeyRequired", noKeyRequired},
            {"fallbackEnabled", fallbackEnabled},
            {"primaryProvider", primaryIds[static_cast<int>(primaryProvider)]},
            {"wordProvider", wordIds[static_cast<int>(wordProvider)]}};
}
LearningSettings LearningSettings::parse(const QVariantMap &values) {
    LearningSettings next;
    next.sourceLanguage = values.value("sourceLanguage", "Turkish").toString().trimmed();
    next.explanationLanguage = values.value("explanationLanguage", "English").toString().trimmed();
    const auto word = wordIds.indexOf(values.value("wordProvider", "ai").toString());
    const auto primary = primaryIds.indexOf(values.value("primaryProvider", "configured").toString());
    if (next.sourceLanguage.isEmpty() || next.explanationLanguage.isEmpty() ||
        next.sourceLanguage.size() > 100 || next.explanationLanguage.size() > 100 || word < 0 || primary < 0)
        throw std::runtime_error("Use language names or codes and a supported provider.");
    next.wordProvider = static_cast<WordProvider>(word);
    next.primaryProvider = static_cast<PrimaryProvider>(primary);
    next.includeSecondary = values.value("includeSecondary", true).toBool();
    next.noKeyRequired = values.value("noKeyRequired", false).toBool();
    next.fallbackEnabled = values.value("fallbackEnabled", false).toBool();
    return next;
}
AppSettings::AppSettings(ProfileStore &profile, QObject *parent) : QObject(parent), store(profile) {
    const auto data = store.snapshot();
    if (AiClient::allowedEndpoint(QUrl(data.value("endpoint").toString())))
        connectionValues.endpoint = data.value("endpoint").toString();
    connectionValues.model = data.value("model").toString(connectionValues.model);
    connectionValues.dictionary = data.value("dictionary").toString(connectionValues.dictionary);
    connectionValues.automaticSubtitles = data.value("automaticSubtitles").toBool();
    try {
        auto savedLearning = data.value("learning").toObject().toVariantMap();
        const auto savedWordProvider = savedLearning.value("wordProvider").toString();
        // Retired translation providers migrate without discarding languages or AI preferences.
        if (savedWordProvider == "google" || savedWordProvider == "microsoft")
            savedLearning.insert("wordProvider", "ai");
        learningValues = LearningSettings::parse(savedLearning);
    } catch (...) {
        statusText =
            "Saved learning settings are invalid. Defaults are active; the original file is preserved.";
    }
    if (!store.readable())
        statusText = store.error();
}
bool AppSettings::commit(const ConnectionSettings &next, const LearningSettings &learning) {
    if (!AiClient::allowedEndpoint(QUrl(next.endpoint)) || next.model.trimmed().isEmpty() ||
        next.model.size() > 200 || buildDictionaryUrl(next.dictionary, "test").isEmpty()) {
        setMessage("Use an HTTPS API base URL, a model ID, and an HTTPS dictionary URL containing %s.");
        return false;
    }
    const QJsonObject fields{{"endpoint", next.endpoint},
                             {"model", next.model},
                             {"dictionary", next.dictionary},
                             {"automaticSubtitles", next.automaticSubtitles},
                             {"learning", learning.json()}};
    if (!store.update(fields)) {
        setMessage(store.error());
        return false;
    }
    connectionValues = next;
    learningValues = learning;
    emit changed();
    setMessage("Settings saved. Existing conversations keep their original context.");
    return true;
}
bool AppSettings::configure(const QString &endpoint, const QString &model, const QString &dictionary,
                            bool automatic) {
    return commit(
        {QUrl(endpoint.trimmed()).toString(QUrl::StripTrailingSlash), model.trimmed(), dictionary, automatic},
        learningValues);
}
bool AppSettings::configureLearning(const QVariantMap &values) {
    try {
        return commit(connectionValues, LearningSettings::parse(values));
    } catch (const std::exception &error) {
        setMessage(QString::fromUtf8(error.what()));
        return false;
    }
}
bool AppSettings::applyDraft(const QVariantMap &draft) {
    try {
        const ConnectionSettings connection{
            QUrl(draft.value("endpoint").toString().trimmed()).toString(QUrl::StripTrailingSlash),
            draft.value("model").toString().trimmed(), draft.value("dictionary").toString(),
            draft.value("automaticSubtitles").toBool()};
        return commit(connection, LearningSettings::parse(draft));
    } catch (const std::exception &error) {
        setMessage(QString::fromUtf8(error.what()));
        return false;
    }
}
QVariantList AppSettings::wordProviders() const {
    return choices(wordIds, {"Contextual AI (configured model)", "Off"});
}
QVariantList AppSettings::primaryProviders() const {
    return choices(primaryIds, {"Configured AI", "GPT-6 Luna"});
}
QVariantList AppSettings::credentialServices() const {
    return choices({"ai", "luna", "subtitles"}, {"Configured AI", "GPT-6 Luna", "OpenSubtitles"});
}

QUrl AppSettings::buildDictionaryUrl(const QString &pattern, const QString &term) {
    if (pattern.count("%s") != 1 || term.trimmed().isEmpty() || pattern.size() > 2048 || term.size() > 1000)
        return {};

    auto encoded = pattern.toUtf8();
    encoded.replace("%s", QUrl::toPercentEncoding(term.trimmed()));
    const auto url = QUrl::fromEncoded(encoded, QUrl::StrictMode);
    if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty())
        return {};

    // Placeholders belong in path/query/fragment, never in the destination host.
    const auto probe = QUrl(QString(pattern).replace(QString("%s"), QString("placeholder")));
    if (url.host() != probe.host())
        return {};

    return url;
}

QUrl AppSettings::dictionaryUrl(const QString &term) {
    const auto url = buildDictionaryUrl(connectionValues.dictionary, term);
    if (url.isEmpty())
        setMessage("The dictionary URL or lookup term is invalid.");
    return url;
}

QString AppSettings::credentialService(const QString &service) const {
    if (service == "ai")
        return connectionValues.endpoint;
    if (service == "luna")
        return "https://api.openai.com/v1";
    if (service == "subtitles")
        return QString("https://api.opensubtitles.com/api/v1");
    return {};
}

void AppSettings::saveServiceKey(const QString &service, const QString &key) {
    const QString endpoint = credentialService(service);
    if (endpoint.isEmpty() || !writer) {
        setMessage("Secure credential storage is unavailable.");
        return;
    }

    if (credentialPending)
        return;
    credentialPending = true;
    setMessage("Saving securely…");
    writer(endpoint, key.toUtf8(), [guard = QPointer<AppSettings>(this)](QString message) {
        if (guard) {
            guard->credentialPending = false;
            guard->setMessage(message);
        }
    });
}

void AppSettings::connectServiceKey(const QString &service) {
    const QString endpoint = credentialService(service);
    if (endpoint.isEmpty() || !authorizer) {
        setMessage("Secure credential access is unavailable.");
        return;
    }

    if (credentialPending)
        return;
    credentialPending = true;
    setMessage("Connecting to the system credential store…");
    authorizer(endpoint, [guard = QPointer<AppSettings>(this)](QString message) {
        if (guard) {
            guard->credentialPending = false;
            guard->setMessage(message);
        }
    });
}
