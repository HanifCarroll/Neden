#pragma once
#include <QNetworkReply>
#include <QString>

inline QString networkFailureMessage(int status, QNetworkReply::NetworkError error,
                                     const QString &operation) {
    QString reason;
    if (status == 401)
        reason = "The saved credentials were rejected. Reconnect the account in Settings.";
    else if (status == 403)
        reason = "The service denied access. Check the account's permissions and allowance.";
    else if (status == 404)
        reason = "The API endpoint or model was not found. Check Settings.";
    else if (status == 429)
        reason = "The service's rate limit or allowance was reached. Wait or check your account before "
                 "trying again.";
    else if (status >= 500)
        reason = "The service is temporarily unavailable. Try again later.";
    else if (status >= 300 && status < 400)
        reason = "The service redirected this request. Check the API address in Settings.";
    else if (status >= 400)
        reason = "The service rejected the request. Check the API address and model in Settings.";
    else {
        switch (error) {
        case QNetworkReply::HostNotFoundError:
            reason = "The server could not be found. Check your connection and the API address in Settings.";
            break;
        case QNetworkReply::SslHandshakeFailedError:
            reason = "A secure connection could not be established. Check the Mac's date, network, and "
                     "certificates.";
            break;
        case QNetworkReply::TimeoutError:
            reason = "The connection timed out. Check your connection, then try again.";
            break;
        case QNetworkReply::ConnectionRefusedError:
            reason = "The server refused the connection. Check the API address or try again later.";
            break;
        default:
            reason = "The connection was interrupted. Check your network, then try again.";
        }
    }
    return operation + ": " + reason + " No automatic retry was made.";
}
