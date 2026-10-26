#include "language_catalog.h"
#include <QLocale>
#include <QMap>
#include <algorithm>

namespace {
struct Language {
    QString code;
    QString name;
    QString subtitleCode;
};
const QList<Language> &languages() {
    static const QList<Language> catalogue = [] {
        // OpenSubtitles uses some non-ISO identifiers. Keep these at the provider boundary.
        // Reference: opensubtitles/vlsub-opensubtitles-com, docs/languages.md.
        const QStringList standardSubtitleCodes = QStringLiteral(
            "ab af sq am ar an hy as eu be bn bs br bg my ca hr cs da nl en eo et fi fr gd gl ka de el "
            "he hi hu is ig id ia ga it ja kn kk km ko ku lv lt lb mk ms ml mr mn nv ne se no oc or fa "
            "pl ps ro ru sr sd si sk sl so es sw sv ta tt te th tr tk uk ur uz vi cy").split(' ');
        const QMap<QString, QString> providerAliases{
            {"az", "az-az"}, {"ast", "at"}, {"yue", "zh-ca"}, {"zh", "zh-cn"},
            {"ext", "ex"}, {"fil", "tl"}, {"mni", "ma"}, {"nb", "no"},
            {"pt", "pt-pt"}, {"sat", "sx"}, {"syr", "sy"}, {"tet", "tm-td"}};
        QList<Language> entries;
        const auto locales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript,
                                                      QLocale::AnyTerritory);
        for (const auto &locale : locales) {
            if (locale.language() == QLocale::C)
                continue;
            const auto code = QLocale::languageToCode(locale.language());
            if (code.isEmpty() || std::any_of(entries.cbegin(), entries.cend(),
                                             [&code](const Language &entry) { return entry.code == code; }))
                continue;
            entries.append({code, QLocale::languageToString(locale.language()),
                            providerAliases.value(code, standardSubtitleCodes.contains(code) ? code : QString())});
        }
        const QList<Language> regionalAndAdditionalLanguages{
            {"pt-BR", "Portuguese (Brazil)", "pt-br"},
            {"pt-MZ", "Portuguese (Mozambique)", "pm"},
            {"zh-TW", "Chinese (Traditional)", "zh-tw"},
            {"es-ES", "Spanish (Spain)", "sp"},
            {"es-419", "Spanish (Latin America)", "ea"},
            {"prs", "Dari", "pr"}, {"cnr", "Montenegrin", "me"},
            {"azb", "South Azerbaijani", "az-zb"}, {"tok", "Toki Pona", "tp"}};
        for (const auto &additional : regionalAndAdditionalLanguages) {
            const auto existing = std::find_if(entries.begin(), entries.end(), [&](const Language &entry) {
                return entry.code == additional.code;
            });
            if (existing == entries.end())
                entries.append(additional);
            else
                *existing = additional;
        }
        std::sort(entries.begin(), entries.end(), [](const Language &left, const Language &right) {
            return left.name.compare(right.name, Qt::CaseInsensitive) < 0;
        });
        return entries;
    }();
    return catalogue;
}
const Language *findLanguage(const QString &value) {
    const auto normalized = value.trimmed();
    for (const auto &entry : languages()) {
        if (normalized.compare(entry.code, Qt::CaseInsensitive) == 0 ||
            normalized.compare(entry.name, Qt::CaseInsensitive) == 0)
            return &entry;
    }
    return nullptr;
}
} // namespace

QString LanguageCatalog::canonicalName(const QString &value, bool allowAutomatic) {
    const auto normalized = value.trimmed();
    if (allowAutomatic && (normalized.compare("auto", Qt::CaseInsensitive) == 0 ||
                           normalized.compare("Auto-detect", Qt::CaseInsensitive) == 0))
        return "Auto-detect";
    const auto *entry = findLanguage(value);
    return entry ? entry->name : QString();
}
QString LanguageCatalog::code(const QString &value) {
    if (canonicalName(value, true) == "Auto-detect")
        return "auto";
    const auto *entry = findLanguage(value);
    return entry ? entry->code : QString();
}
QString LanguageCatalog::systemLanguage() {
    const auto locale = QLocale::system();
    if (locale.language() == QLocale::Chinese && locale.script() == QLocale::TraditionalHanScript)
        return canonicalName("zh-TW");
    const auto localeCode = locale.name().replace('_', '-');
    auto name = canonicalName(localeCode);
    if (name.isEmpty())
        name = canonicalName(QLocale::languageToCode(QLocale::system().language()));
    return name.isEmpty() ? QString("English") : name;
}
QVariantList LanguageCatalog::options(bool includeAutomatic) {
    QVariantList result;
    if (includeAutomatic)
        result.append(QVariantMap{{"id", "auto"}, {"name", "Auto-detect"}});
    for (const auto &entry : languages())
        result.append(QVariantMap{{"id", entry.code}, {"name", entry.name}});
    return result;
}
QVariantList LanguageCatalog::subtitleOptions() {
    QVariantList result;
    for (const auto &entry : languages()) {
        if (!entry.subtitleCode.isEmpty())
            result.append(QVariantMap{{"id", entry.subtitleCode}, {"name", entry.name}});
    }
    return result;
}
QString LanguageCatalog::subtitleCode(const QString &value) {
    const auto *entry = findLanguage(value);
    return entry ? entry->subtitleCode : QString();
}
bool LanguageCatalog::supportsSubtitleCode(const QString &code) {
    return !code.isEmpty() && std::any_of(languages().cbegin(), languages().cend(),
                                        [&code](const Language &entry) { return entry.subtitleCode == code; });
}
