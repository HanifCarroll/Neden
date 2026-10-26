#pragma once
#include <QString>
#include <QVariantList>

// One vocabulary for saved preferences, language pickers and provider requests.
namespace LanguageCatalog {
QString canonicalName(const QString &nameOrCode, bool allowAutomatic = false);
QString code(const QString &nameOrCode);
QString systemLanguage();
QVariantList options(bool includeAutomatic = false);
QVariantList subtitleOptions();
QString subtitleCode(const QString &nameOrCode);
bool supportsSubtitleCode(const QString &code);
} // namespace LanguageCatalog
