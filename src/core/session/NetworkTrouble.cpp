// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/NetworkTrouble.cpp  (NereusSDR)
// =================================================================
//
// See NetworkTrouble.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/NetworkTrouble.h"

namespace NereusSDR::NetworkTrouble {

QString wordsForTlsErrors(const QList<QSslError>& errors)
{
    bool otherName = false;
    bool untrustedAuthority = false;
    for (const QSslError& error : errors) {
        switch (error.error()) {
        case QSslError::HostNameMismatch:
            otherName = true;
            break;
        case QSslError::UnableToGetIssuerCertificate:
        case QSslError::UnableToGetLocalIssuerCertificate:
        case QSslError::UnableToVerifyFirstCertificate:
        case QSslError::SelfSignedCertificate:
        case QSslError::SelfSignedCertificateInChain:
        case QSslError::CertificateUntrusted:
            untrustedAuthority = true;
            break;
        default:
            break;
        }
    }
    // Either failure can have several causes. The certificate alone cannot
    // establish that a captive portal or a TLS inspection proxy is present.
    if (otherName) {
        return QStringLiteral("The secure connection answered with a certificate for another "
                              "name. A Wi-Fi sign-in page is one possible cause. Check whether "
                              "this network needs browser sign-in, then try again.");
    }
    if (untrustedAuthority) {
        return QStringLiteral("This computer does not trust the certificate for the secure "
                              "connection. Network inspection is one possible cause. Check "
                              "the network's certificate policy or try another network.");
    }
    return QString();
}

QString proxyNeedsLoginWords()
{
    return QStringLiteral("This network's proxy needs a login, which NereusSDR can't provide.");
}

QString wordsForSocketError(QAbstractSocket::SocketError error)
{
    // Qt reports this once the proxy has answered 407 and nothing gave the
    // challenge a user name and password; NereusSDR never does.
    return error == QAbstractSocket::ProxyAuthenticationRequiredError ? proxyNeedsLoginWords()
                                                                      : QString();
}

} // namespace NereusSDR::NetworkTrouble
