// =================================================================
// src/core/session/IceDiagnostics.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. An opt-in log of the Core's ICE
// checks, for one diagnostic run: the candidates each peer gathers and
// admits (the loopback tunnel's included), libjuice's pair checks through
// libdatachannel's log callback, and the pair each peer selects. Every line
// passes through IceAddressRedactor first, which replaces every IPv4 and
// IPv6 literal with a token. Hostnames, mDNS candidate names (".local")
// and the TURN server's host name are not redacted. The ICE password and
// username fragment, and the STUN username, realm and nonce, are hidden.
//
// Off unless the environment variable NEREUS_ICE_DIAG is set to 1, true,
// on or yes when the Core starts. Off, the library logger is never installed and
// every call here returns after one atomic load.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Created for the 5G media-path diagnosis. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Review fix: says which addresses are redacted and which
//                 are not, the "yes" switch value, and isSecretLine(). J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QNetworkAddressEntry>
#include <QString>

#include <functional>
#include <sstream>

namespace rtc {
class PeerConnection;
}

namespace NereusSDR {

/// Replaces every IPv4 and IPv6 literal in a line with a token that names
/// its family, a per-run index and, once known, its ICE candidate type:
/// "<v4#3 srflx>", "<v6#1 host>". The same address keeps its token for the
/// life of the redactor. The Core's own IPv6 addresses carry "stable" or
/// "temporary" (a privacy address), "deprecated", and "eui64" when the
/// interface identifier is a MAC-derived one. Loopback reads "loopback",
/// the unspecified address "unspecified". Ports and an interface scope
/// ("%en0") stay; the brackets of "[v6]:port" go with the address.
/// Thread-safe.
class IceAddressRedactor {
public:
    using LocalSource = std::function<QList<QNetworkAddressEntry>()>;

    /// The Core's own addresses, for the markers.
    void setLocalAddresses(const QList<QNetworkAddressEntry>& entries);
    /// Where the Core's addresses are read again when an IPv6 address is
    /// first seen (a renumbered or new privacy address). Empty: never.
    void setLocalSource(LocalSource source);

    QString redact(const QString& text);
    /// The Core's own addresses as tokens, IPv6 first: which of them are
    /// stable and which temporary, to set against the gathered candidates.
    QString localSummary();

private:
    struct Local {
        bool temporary = false;
        bool deprecated = false;
        bool eui64 = false;
    };
    struct Known {
        QString name;   // "v4#1", "v6#2"
        QString type;   // "host", "srflx", "prflx", "relay" or empty
    };
    struct Match {
        int start = 0;
        int end = 0;       // exclusive
        QString key;       // canonical; empty for loopback and unspecified
        QString fixed;     // "loopback" or "unspecified"
        bool ipv6 = false;
    };

    void loadLocalLocked(const QList<QNetworkAddressEntry>& entries);
    bool findInRunLocked(const QString& run, QChar before, QChar after, int from,
                         Match* match) const;
    QString keyOfLocked(const QString& text) const;
    void learnTypesLocked(const QString& line);
    void learnLocked(const QString& addressText, const QString& type);
    QString tokenLocked(const Match& match);

    QMutex m_mutex;
    QHash<QString, Known> m_known;
    QHash<QString, Local> m_local;
    QList<QString> m_localOrder;
    LocalSource m_localSource;
    int m_nextV4 = 1;
    int m_nextV6 = 1;
};

namespace IceDiagnostics {

/// The switch: set to 1 (or true, on, yes) in the Core's environment for a
/// run. Case and surrounding spaces do not matter.
inline constexpr char kEnvironmentVariable[] = "NEREUS_ICE_DIAG";

using Sink = std::function<void(const QString& line)>;

/// Whether an environment value turns the log on.
bool switchRequested(const QByteArray& value);
/// Whether the log is on. One atomic load.
bool enabled();
/// Reads the switch and, when it is on, installs libdatachannel's logger
/// at its verbose level with every line redacted, and writes the Core's
/// own addresses as tokens. Called once, before any peer is made (the
/// library reads its level when each ICE agent is made).
void installFromEnvironment();
/// Test seam: on with its lines handed to `sink` instead of the Core log,
/// the markers read from `localSource` when one is given; an empty sink
/// turns the log off again.
void installForTest(Sink sink, IceAddressRedactor::LocalSource localSource = {});

/// libjuice's per-message STUN username, realm and nonce lines: secrets,
/// left out whole.
bool isSecretLine(const QString& line);
/// libdatachannel and libjuice lines that come once per packet (sends,
/// receives, SCTP, SRTP) and say nothing about the checks; left out.
bool isLibraryNoise(const QString& line);

/// One redacted line tagged with the path it is about ("control", "media").
void logPath(const char* path, const QString& text);
/// The pair `peer` has selected, redacted, when it has one.
void logSelectedPair(const char* path, rtc::PeerConnection& peer);

/// A libdatachannel state ("connected", "checking") by its own name.
template <typename State>
QString stateName(State state)
{
    std::ostringstream out;
    out << state;
    return QString::fromStdString(out.str());
}

} // namespace IceDiagnostics

} // namespace NereusSDR
