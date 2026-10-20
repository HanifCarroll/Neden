#include "credentials.h"
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <LocalAuthentication/LocalAuthentication.h>

QByteArray readCredentialWithoutInteraction(const QString &serviceName, const QString &account) {
    const auto service = serviceName.toUtf8();
    LAContext *authentication = [[LAContext alloc] init];
    authentication.interactionNotAllowed = YES;
    NSDictionary *query = @{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : [NSString stringWithUTF8String:service.constData()],
        (__bridge id)kSecAttrAccount : [NSString stringWithUTF8String:account.toUtf8().constData()],
        (__bridge id)kSecReturnData : @YES,
        (__bridge id)kSecMatchLimit : (__bridge id)kSecMatchLimitOne,
        (__bridge id)kSecUseAuthenticationContext : authentication
    };

    // 2. Only an explicit Connect action may display the system access dialog.
    Boolean previousInteraction = true;
    SecKeychainGetUserInteractionAllowed(&previousInteraction);
    SecKeychainSetUserInteractionAllowed(false);
    CFTypeRef result = nullptr;
    const auto status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    SecKeychainSetUserInteractionAllowed(previousInteraction);
    if (status != errSecSuccess || !result)
        return {};

    NSData *data = CFBridgingRelease(result);
    return QByteArray(static_cast<const char *>(data.bytes), data.length);
}
