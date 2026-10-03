#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SystemProxy.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16; the options survey's B.5):
// the proxy the computer's own settings name for a WebSocket, so the
// remote access service, the web relay's leg and a direct wss connection
// reach out through an explicit HTTP proxy (CONNECT) where the network
// requires one. Qt's system configuration: the macOS and Windows settings,
// PAC files where Qt supports them, and the http_proxy, https_proxy and
// no_proxy variables on Linux. A wss:// address is asked for as https://
// and ws:// as http://, the way a browser asks. No proxy (a direct
// connection) when the settings name none, or only kinds a WebSocket
// cannot use, and always for this computer's own loopback addresses.
// A proxy that demands a login is given none (NereusSDR has no proxy login
// yet); each caller says so in plain words (NetworkTrouble::
// proxyNeedsLoginWords) instead of failing quietly.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29: a proxy that demands a login is named (JJ's ruling of
//               2026-09-28). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QNetworkProxy>
#include <optional>
#include <QUrl>

namespace NereusSDR::SystemProxy {

/// The proxy for a WebSocket to `url` (ws:// or wss://), or
/// QNetworkProxy::NoProxy.
QNetworkProxy forUrl(const QUrl& url);
/// Process-local test seam; never changes the computer's proxy settings.
void setProxyForTest(const std::optional<QNetworkProxy>& proxy);

} // namespace NereusSDR::SystemProxy
