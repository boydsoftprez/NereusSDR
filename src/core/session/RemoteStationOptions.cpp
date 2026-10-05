// =================================================================
// src/core/session/RemoteStationOptions.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 20.
//
// See RemoteStationOptions.h for why the AppSettings key literals are not
// in this file.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 20: --station
//                                    and --token, and the remote-mode GUI
//                                    gate. AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/session/RemoteStationOptions.h"
#include "core/session/RendezvousWire.h"

#include <QLatin1String>
#include <QUrl>

namespace NereusSDR {

// 2026-10-01: Addressless paired Cores can retry authenticated listeners.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.
bool RemoteStationOptions::hasAuthenticatedDirectAddresses() const
{
    if (identityFingerprint.size() != 32 || allowUnpinned) { return false; }
    for (const QString& address : directCandidates + cachedAddresses + coreAddresses) {
        if (isValidStationUrl(address)) { return true; }
    }
    return false;
}

bool RemoteStationOptions::isValidRemoteTarget(QString* whyNot) const
{
    if (!url.isEmpty()) {
        return isValidStationUrl(url, whyNot);
    }
    if (identityFingerprint.size() != 32
        || (!hasAuthenticatedDirectAddresses() && !RendezvousWire::isRendezvousId(rendezvousId))
        || !token.isEmpty() || !fingerprint.isEmpty() || allowUnpinned) {
        if (whyNot) {
            *whyNot = QStringLiteral("Pair with the Core again to reach it through remote access.");
        }
        return false;
    }
    return true;
}

bool RemoteStationOptions::isValidStationUrl(const QString& candidate, QString* whyNot)
{
    if (candidate.isEmpty()) {
        if (whyNot != nullptr) {
            *whyNot = QStringLiteral("Core address is empty.");
        }
        return false;
    }

    const QUrl url(candidate, QUrl::StrictMode);
    if (!url.isValid()) {
        if (whyNot != nullptr) {
            *whyNot = QStringLiteral("Core address is not a valid URL: %1")
                          .arg(url.errorString());
        }
        return false;
    }

    const QString scheme = url.scheme();
    if (scheme != QLatin1String("wss") && scheme != QLatin1String("ws")) {
        if (whyNot != nullptr) {
            // Naming the scheme back is the point: an operator who typed
            // https:// gets told which two words the field wants, rather
            // than a QWebSocket connect failure several seconds later.
            *whyNot = QStringLiteral(
                          "Core address must start with wss:// or ws:// "
                          "(got \"%1\").")
                          .arg(scheme.isEmpty() ? QStringLiteral("no scheme")
                                                : scheme);
        }
        return false;
    }

    if (url.host().isEmpty()) {
        if (whyNot != nullptr) {
            *whyNot = QStringLiteral("Core address has no host.");
        }
        return false;
    }

    return true;
}

} // namespace NereusSDR
