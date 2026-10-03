#pragma once
// =================================================================
// src/core/session/SessionEndReasons.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// The two session ends an app needs its own buttons for, worded in one
// place (R-R3-21, R-R3-38, R-IOS-01): another app took the Core over, and
// the link versions are too far apart. The Core formats them for its
// `session.end` reason (StationServer), the desktop client formats the
// version reason for a Core it refuses itself (StationClient::handleHello),
// and the desktop client parses the reason back (stationEndReport) to
// offer Take it back or Check for updates.
//
// The Core also sends an end code (`code` on `session.end` and
// `auth.result`, station link section 12.4; iPhone app Task 12). read()
// takes the code first (iPhone app Task 18) and falls back to the words
// for an older Core, which sends none.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R3 completion carry, review finding
//                                    I1 (R-R3-21, R-R3-38, R-IOS-01): the
//                                    takeover and version reasons, formatted
//                                    and parsed in one place, worded with
//                                    "Core". AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Final review M1 (R-R3-38, R-IOS-01):
//                                    parse also reads an older Core's
//                                    wordings. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 18 (R-IOS-08,
//                                    R-IOS-17): read() chooses by the end
//                                    code, the words stay the fallback.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QList>
#include <QString>
#include <QtGlobal>

namespace NereusSDR::SessionEndReasons {

/// Another app took the Core over. `otherApp` is how the Core names it,
/// its network address and port (WebSocketTransport::peerDescription(),
/// "address:port").
QString takenOver(const QString& otherApp);

/// The Core and the app share no link major. Names each side's newest
/// version and the side to update, for example "This Core runs link
/// version 1 and this app runs version 3. Update the Core."
QString versionRefused(const QList<quint16>& coreMajors, const QList<quint16>& appMajors);

struct Parsed {
    enum class Kind {
        Other,          ///< none of the below
        TakenOver,      ///< takenOver(), or the code takenOver
        VersionRefused, ///< versionRefused(), or the code linkVersion
        // iPhone app Task 18, by code only (no older Core sends these):
        DeviceRemoved,   ///< deviceRemoved or deviceNotPaired
        PairingRequired, ///< pairingRequired
        IdentityChanged, ///< identityChanged (the app's own end)
    };
    Kind kind = Kind::Other;
    /// TakenOver only: the other app's network address without its port,
    /// IPv4 written as IPv4. Empty when the Core named no address (it
    /// writes "<unknown>" or "<detached>" then).
    QString otherAppAddress;
    /// VersionRefused only: each side's newest link major.
    int coreMajor = -1;
    int appMajor = -1;
};

/// Reads a reason formatted by takenOver() or versionRefused(), or an
/// older Core's wording of the same two: "Displaced by a newer
/// authenticated connection from address:port" and "Protocol major version
/// mismatch: station speaks M.m, client speaks M.m. A differing major means
/// an incompatible wire contract."
Parsed parse(const QString& reason);

/// iPhone app Task 18: the end as the Core coded it (SessionEndCode).
/// An empty code is an older Core's end, read by parse(). For takenOver
/// and linkVersion the reason's words still give the other app's address
/// and the two versions, when they are there. A code this app does not
/// know is Other.
Parsed read(const QString& code, const QString& reason);

} // namespace NereusSDR::SessionEndReasons
