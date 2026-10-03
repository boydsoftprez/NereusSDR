// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#pragma once
// =================================================================
// src/core/session/RemoteStationOptions.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 20.
//
// Where a GUI process gets its station address and token from, and the
// one rule for what counts as a usable station URL.
//
// Deliberately a plain value struct with no AppSettings dependency and no
// key literals. The keys live in the Setup page that owns the field group
// (src/gui/setup/CatNetworkSetupPages.cpp) and nowhere else, because
// tests/tst_settings_scope.cpp's completeness sweep asserts that every
// AppSettings key literal appearing in src/core or src/models classifies
// Station -- and the station URL and its token are the two keys in this
// tree that MUST NOT: they are the client's own address book, and writing
// them into the shared station store would hand one operator's
// credentials to every other client of the same daemon. Keeping the
// literals out of src/core keeps that from ever becoming a question.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 20: --station
//                                    and --token, and the remote-mode GUI
//                                    gate. AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 18 (R-IOS-08): the
//                                    paired Core's identity fingerprint.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 27 (R-IOS-16):
//                                    the Core's last good addresses.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 28 fix wave
//                                    (R-IOS-16): the Core's recorded
//                                    controlChannelVersion. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 (R-IOS-16): the Core's rendezvous
//               id, its relay setting as last told, and the operator's
//               choice to reach it through the internet service. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>

namespace NereusSDR {

/// The station a GUI process should drive. A fully empty object is local
/// direct mode (the default, and the only mode before R2); a paired
/// identity and remote access route can also be remote without a URL.
struct RemoteStationOptions {
    /// `wss://host:port` (or `ws://` for a loopback bench run). Empty
    /// means no direct address. When no paired route is present either,
    /// construct a Role::Local RadioModel as releases before R2 did.
    QString url;

    /// The daemon's shared token, as printed by `nereusd` on first run.
    /// Never generated here and never sent anywhere but the station named
    /// by `url`.
    QString token;

    /// SHA-256 fingerprint of the certificate to pin, or empty.
    QString fingerprint;

    /// Accept the station's self-signed certificate without a pinned
    /// fingerprint. Bench convenience; see StationClient::connectToStation.
    bool allowUnpinned = false;

    /// iPhone app Task 18 (R-IOS-08): the fingerprint (SHA-256 of the
    /// SubjectPublicKeyInfo DER, 32 bytes) of the Core identity key this
    /// computer paired with, or empty for a Core it has not. When set, the
    /// Core is trusted by that key: its hello must show it, the certificate
    /// binding must verify for the certificate this connection presents,
    /// and this computer signs in with its own device key. The pin and the
    /// token are then not used (the link document, sections 3.4 and 3.5).
    QByteArray identityFingerprint;

    /// iPhone app plan Task 27 (R-IOS-16; the pairing design, section 5.3):
    /// where this computer last reached the Core (station URLs, the most
    /// recent first, at most kMaxCachedAddresses), tried before `url` so a
    /// reconnect never needs the remote access service.
    static constexpr int kMaxCachedAddresses = 4;
    QStringList cachedAddresses;
    /// Authenticated devices.coreAddresses, scoped to identityFingerprint.
    /// Separate from operator URL and four successful endpoints. Link 7.1.
    QStringList coreAddresses;
    /// Transient ordinary-attempt snapshot; never serialized as successful history.
    QStringList directCandidates;


    /// iPhone app plan Task 28 fix wave (R-IOS-16; the safety review's
    /// Important 5): the controlChannelVersion the Core sent at this
    /// computer's last sign-in, or -1 before any (a Core paired through a
    /// mailbox has had no session yet). Connecting from anywhere is offered
    /// to a Core that sent 1 or has none recorded, and refused with
    /// kUpdateCoreForServiceReason to one that sent 0
    /// (serviceConnectRefusal()).
    int controlChannelVersion = -1;
    // Wall-clock time of the last authenticated ordinary-session snapshot
    // that reported version 0. Missing or invalid time makes 0 unknown.
    qint64 negativeControlObservedMs = -1;
    static constexpr qint64 kNegativeControlLifetimeMs = 5 * 60 * 1000;
    int effectiveControlChannelVersion(qint64 nowMs) const
    {
        if (controlChannelVersion != 0) { return controlChannelVersion; }
        if (negativeControlObservedMs < 0 || negativeControlObservedMs > nowMs
            || nowMs - negativeControlObservedMs >= kNegativeControlLifetimeMs) {
            return -1;
        }
        return 0;
    }
    static constexpr const char* kUpdateCoreForServiceReason =
        "Update the Core to reach it from anywhere.";
    /// Why connecting through the remote access service is not offered for
    /// this Core, in plain words, or empty when it is: a Core this
    /// computer has not paired with (it is introduced by its identity key),
    /// one whose last session declared no control channel, or (Task 29 fix
    /// wave, review Minor 4) one whose service name (rendezvousId, learned
    /// at a sign-in) this computer does not know yet.
    QString serviceConnectRefusal(qint64 nowMs = QDateTime::currentMSecsSinceEpoch()) const
    {
        if (identityFingerprint.isEmpty()) {
            return QStringLiteral("Pair with the Core to reach it from anywhere.");
        }
        if (effectiveControlChannelVersion(nowMs) == 0) {
            return QString::fromLatin1(kUpdateCoreForServiceReason);
        }
        if (rendezvousId.isEmpty()) {
            return QString::fromLatin1(kSignInOnceForServiceReason);
        }
        return QString();
    }
    static constexpr const char* kSignInOnceForServiceReason =
        "Connect to the Core once to reach it from anywhere.";

    /// iPhone app plan Task 29 (R-IOS-16; the link document, section 21):
    /// the Core's rendezvous id (learned from its hello at a sign-in), what
    /// its last session said of the relay (`relayAllowed`: -1 not recorded,
    /// 0 turned off, 1 allowed), and whether the operator lets this
    /// computer reach it through the internet service beside its
    /// addresses (on by default). With all three, a connect races the
    /// service with the addresses.
    QString rendezvousId;
    int relayAllowed = -1;
    bool reachFromAnywhere = true;

    /// Remote intent includes a paired service route without a direct URL.
    /// Even malformed intent must not silently construct a local Core.
    bool isRemote() const
    {
        return !url.isEmpty() || !identityFingerprint.isEmpty() || !rendezvousId.isEmpty();
    }

    /// A direct URL keeps its existing rules. Without one, only a paired
    /// identity and service route can identify a remote Core.
    bool hasAuthenticatedDirectAddresses() const;
    bool isValidRemoteTarget(QString* whyNot = nullptr) const;

    /// Whether `candidate` is a station URL this build can dial.
    ///
    /// Accepts ws:// and wss:// with a non-empty host. Rejects everything
    /// else, including http/https, which is the mistake worth catching by
    /// name: a QWebSocket handed an http:// URL fails at connect time with
    /// a message that does not say why.
    ///
    /// `whyNot`, when non-null, receives a one-line operator-facing reason
    /// on failure and is left untouched on success.
    static bool isValidStationUrl(const QString& candidate, QString* whyNot = nullptr);
};

} // namespace NereusSDR
