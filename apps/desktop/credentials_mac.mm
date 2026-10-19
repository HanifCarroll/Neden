#include "credentials.h"
#include <QHash>
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <LocalAuthentication/LocalAuthentication.h>

namespace {
// Process-local credential storage; neither QML nor persisted settings receives it.
QHash<QString, QByteArray> authorizedKeys;
struct CredentialResult { OSStatus status; QByteArray key; };

CredentialResult lookupCredential(const QString &endpoint, bool interactive, const QString &account = "api-key") {

    // 1. Address only the existing endpoint-bound IINA credential.
    const auto service = ("io.github.hanifcarroll.iina-language-learning - api:" + endpoint).toUtf8();
    LAContext *authentication = [[LAContext alloc] init];
    authentication.interactionNotAllowed = !interactive;
    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: [NSString stringWithUTF8String:service.constData()],
        (__bridge id)kSecAttrAccount: [NSString stringWithUTF8String:account.toUtf8().constData()],
        (__bridge id)kSecReturnData: @YES,
        (__bridge id)kSecMatchLimit: (__bridge id)kSecMatchLimitOne,
        (__bridge id)kSecUseAuthenticationContext: authentication
    };

    // 2. Only an explicit Connect action may display the system access dialog.
    Boolean previousInteraction = true;
    SecKeychainGetUserInteractionAllowed(&previousInteraction);
    SecKeychainSetUserInteractionAllowed(interactive);
    CFTypeRef result = nullptr;
    const auto status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    SecKeychainSetUserInteractionAllowed(previousInteraction);
    if (status != errSecSuccess || !result) return {status, {}};

    // 3. Validate secret bytes without placing them in an error or presentation property.
    NSData *data = CFBridgingRelease(result);
    QByteArray key(static_cast<const char *>(data.bytes), data.length);
    if (key.isEmpty() || key.size() > (account == "login" ? 8192 : 512) || key.contains('\r') || key.contains('\n')) {
        key.fill(0);
        return {errSecDecode, {}};
    }
    return {errSecSuccess, key};
}
}

QByteArray readSavedCredential(const QString &endpoint) {
    if (authorizedKeys.contains(endpoint)) return authorizedKeys.value(endpoint);
    return lookupCredential(endpoint, false).key;
}

QByteArray readSavedSubtitleLogin() {
    return authorizedKeys.value("opensubtitles-login");
}

QString authorizeSavedCredential(const QString &endpoint) {

    // 1. Authorize only the credential(s) needed by the selected service.
    const bool subtitles = endpoint == "https://api.opensubtitles.com/api/v1";
    const QStringList accounts = subtitles ? QStringList{"api-key", "login"} : QStringList{"api-key"};
    for (const auto &account : accounts) {
        const auto result = lookupCredential(endpoint, true, account);
        if (result.status == errSecSuccess && !result.key.isEmpty()) {
            authorizedKeys.insert(account == "login" ? "opensubtitles-login" : endpoint, result.key);
            continue;
        }
        if (result.status == errSecItemNotFound) return "No saved Neden credential exists for this endpoint/account.";
        if (result.status == errSecUserCanceled || result.status == errSecAuthFailed)
            return "Keychain access was cancelled or denied. No request was sent.";
        return QString("Keychain access failed (status %1). No request was sent.").arg(result.status);
    }

    // 2. Report access only; the selected action will make the network request.
    return "Connected. The saved credentials are available for this app session.";
}

QString saveCredential(const QString &endpoint, QByteArray key) {

    // 1. Validate secret bytes without including them in a status or log.
    if (key.isEmpty() || key.size() > 512 || key.contains('\r') || key.contains('\n')) {
        key.fill(0);
        return "Use a nonempty API key of at most 512 bytes without line breaks.";
    }

    // 2. Update the existing endpoint-bound item or create it in the same native Keychain.
    const auto service = ("io.github.hanifcarroll.iina-language-learning - api:" + endpoint).toUtf8();
    NSDictionary *query = @{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: [NSString stringWithUTF8String:service.constData()],
        (__bridge id)kSecAttrAccount: @"api-key"
    };
    NSData *data = [NSData dataWithBytes:key.constData() length:key.size()];
    NSDictionary *attributes = @{(__bridge id)kSecValueData: data};
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)attributes);
    if (status == errSecItemNotFound) {
        NSMutableDictionary *item = [query mutableCopy];
        [item addEntriesFromDictionary:attributes];
        status = SecItemAdd((__bridge CFDictionaryRef)item, nullptr);
    }
    if (status == errSecSuccess) authorizedKeys.insert(endpoint, key);
    key.fill(0);
    return status == errSecSuccess ? "Key saved securely in Keychain."
        : QString("Keychain could not save the key (status %1). Nothing was written to preferences.").arg(status);
}
