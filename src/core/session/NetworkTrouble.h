#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/NetworkTrouble.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16; the options survey's B.6 and
// B.7): plain words for two networks that break the secure connection to
// the remote access service before it starts.
//
//   - A certificate for another name can indicate a captive portal, but
//     can also be a server or DNS configuration problem.
//   - An untrusted issuer can indicate network inspection, but can also
//     be an incomplete chain or a local trust-store problem.
//
// Only the words: the service is never pinned (the system's trusted
// authorities decide, as before), and the Core's identity check, which
// rides inside the session and survives any relay, is unchanged. No probe
// is sent; the operator is told what the failure looks like.
//
// And a third (JJ's ruling of 2026-09-28): a proxy that demands a login.
// NereusSDR has no proxy login to give (a later ruling may add one), so
// every WebSocket the computer's proxy settings send through a proxy
// (SystemProxy::forUrl) says so in plain words instead of failing quietly.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: wordsForSocketError, the proxy that needs a login. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QAbstractSocket>
#include <QList>
#include <QSslError>
#include <QString>

namespace NereusSDR::NetworkTrouble {

/// The operator's words for a secure connection that failed with
/// `errors`; empty when they point at neither case.
QString wordsForTlsErrors(const QList<QSslError>& errors);

/// The operator's words when a proxy on the way demands a login:
/// "This network's proxy needs a login, which NereusSDR can't provide."
QString proxyNeedsLoginWords();

/// The operator's words for a WebSocket that failed with `error`: the
/// proxy's login (QAbstractSocket::ProxyAuthenticationRequiredError);
/// empty for any other error.
QString wordsForSocketError(QAbstractSocket::SocketError error);

} // namespace NereusSDR::NetworkTrouble
