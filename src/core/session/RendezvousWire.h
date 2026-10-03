#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousWire.h  (NereusSDR)
// =================================================================
//
// The rendezvous version 1 wire, as the Core's station role and the
// desktop's client role speak it (iPhone app plan Task 27, R-IOS-08,
// R-IOS-16). The authority is
// docs/architecture/2026-09-23-rendezvous-v1.md; the section numbers below
// are that document's.
//
//   - Identity values (section 4): the station's rendezvous id, and the
//     registration and introduction transcripts a station or device signs.
//   - Messages (section 5): one JSON object per WebSocket text frame, each
//     field of the kind section 5.2 gives. decode() reads a message in one
//     of the four directions by that direction's table; encode() writes
//     one, and refuses (returns empty) anything that breaks a field's kind
//     or the whole-message cap of section 2, which is the sender's rule:
//     such a message is not sent.
//
// A station or client ignores a key it does not know in a kind it knows,
// and a kind it does not know (section 5.1); decode() reports an unknown
// kind as Kind::Unknown with success false, and the caller logs and
// carries on.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 fix wave (R-IOS-16): relay.grant
//               (section 12.1), from the service to a station and a client.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <optional>

namespace NereusSDR::RendezvousWire {

/// The rendezvous version this build speaks (section 6.1).
inline constexpr int kVersion = 1;
/// Section 2: the most a station or client sends in one message, encoded.
inline constexpr qsizetype kMaxOutgoingMessageBytes = 131072;
/// Section 2: the most a station or client accepts from the service.
inline constexpr qsizetype kMaxIncomingMessageBytes = 262144;
/// Section 5.2's field caps, in UTF-8 bytes of the decoded string.
inline constexpr qsizetype kMaxSdpBytes = 65536;
inline constexpr qsizetype kMaxCandidateBytes = 4096;
inline constexpr qsizetype kMaxBodyBytes = 65536;
inline constexpr qsizetype kMaxReasonBytes = 1024;
inline constexpr qsizetype kMaxCodeBytes = 64;
inline constexpr qsizetype kMaxUrlBytes = 512;
inline constexpr int kMaxUrls = 8;
inline constexpr qsizetype kMaxTurnUsernameBytes = 512;
inline constexpr qsizetype kMaxTurnPasswordBytes = 128;
inline constexpr qint64 kMaxTurnExpires = 4294967295LL;
inline constexpr qint64 kMaxRetryAfterMs = 2147483647LL;
/// Section 5.2's `relayUrl` and `relayToken`: 1 to 512 bytes each.
inline constexpr qsizetype kMaxRelayUrlBytes = 512;
inline constexpr qsizetype kMaxRelayTokenBytes = 512;
/// Section 5.2: nameplates run from 1 to this (PairingCode::kMaxNameplate).
inline constexpr int kMaxNameplate = 999999;
/// Section 4.2: the id is this many characters of `a-z` and `2-7`.
inline constexpr int kIdLength = 26;
/// Section 4 and 5.2: the byte lengths of the binary fields.
inline constexpr int kNonceBytes = 32;
inline constexpr int kIntroBytes = 16;
inline constexpr int kDeviceIdBytes = 32;
inline constexpr int kSignatureBytes = 64;
inline constexpr int kPublicKeyBytes = 91;

/// Relay credentials (section 8), exactly the four keys the service sends.
struct Turn {
    QString username;
    QString password;
    qint64 expires = 0;
    QStringList urls;

    bool operator==(const Turn&) const = default;
};

enum class Kind {
    Unknown,
    Hello,
    Register,
    Challenge,
    Prove,
    Registered,
    Introduce,
    Introduction,
    Answer,
    Credentials,
    Candidate,
    IntroductionEnd,
    NameplateClaim,
    Nameplate,
    NameplateRelease,
    NameplateReleased,
    MailboxOpen,
    MailboxOpened,
    Mailbox,
    MailboxClose,
    MailboxClosed,
    Error,
    /// Section 12.1: the WebSocket relay's grant, from the service only.
    RelayGrant,
};

/// Who sends a message to whom (section 5.3's four tables).
enum class Direction {
    StationToService,
    ClientToService,
    ServiceToStation,
    ServiceToClient,
};

/// One message, any kind. Only the fields its kind lists are meaningful.
struct Message {
    Kind kind = Kind::Unknown;

    int watchRelayVersion = 0;  // optional hello/register/introduce; zero means absent
    QString watchToken;        // optional relay.grant; opaque, never logged
    int version = 0;            // hello
    QByteArray nonce;           // hello, challenge, introduction (32 bytes)
    QStringList stun;           // hello

    QString id;                 // register, registered, introduce (a rendezvous id)
    QByteArray publicKey;       // register (SPKI DER, 91 bytes)
    QByteArray signature;       // prove (64 bytes)

    QByteArray intro;           // introduction/credentials/candidate/introduction.end
                                // `from`, answer/candidate `to` (16 bytes)
    QByteArray device;          // introduce, introduction (a device id, 32 bytes)
    QByteArray deviceSignature; // introduce, introduction (64 bytes)
    QString sdp;                // introduce/introduction `offer`, answer `answer`

    bool turnRequested = false; // a station's answer `turn`
    std::optional<Turn> turn;   // the service's answer and credentials `turn`; nullopt is null

    QString candidate;          // candidate (empty: the end of candidates)
    int nameplate = 0;          // nameplate, mailbox.open, mailbox.opened
    QString body;               // mailbox

    QString code;               // introduction.end, mailbox.closed, error
    QString reason;             // error
    qint64 retryAfterMs = 0;    // error

    QString relayUrl;           // relay.grant `url` (wss://)
    QString relayToken;         // relay.grant `token`: opaque, never logged
    qint64 relayExpires = 0;    // relay.grant `expires` (Unix seconds)
};

/// Section 12.1: a relay grant as the service sent it. `token` is opaque:
/// it goes only to the relay at `url` and is never logged or put in a URL.
struct RelayGrant {
    QString url;
    QString token;
    qint64 expires = 0;
    QString watchToken; // Optional separate watch grant, never a primary token.
};

/// The kind's wire name ("mailbox.open"), empty for Unknown.
QString kindName(Kind kind);

/// Reads `wire` as a message travelling in `direction`. False, with `why`
/// set, when it is not one: JSON that does not parse, a kind this
/// direction does not carry (`out->kind` is then Unknown), or a listed
/// field that is missing or not of its kind. Keys a kind does not list are
/// ignored.
bool decode(Direction direction, const QByteArray& wire, Message* out, QString* why = nullptr);

/// Writes a message travelling in `direction`, compact JSON with only the
/// listed keys. Empty when the direction does not carry the kind, a field
/// breaks its kind, or the whole message would pass its cap (for what a
/// station or client sends, kMaxOutgoingMessageBytes): the caller does not
/// send it. A station or client writes only its own direction; the
/// service's directions are here for the conformance runners, which play
/// the service.
QByteArray encode(Direction direction, const Message& message);

/// Section 4.2: base32 (RFC 4648, lowercase, no padding) of
/// SHA-256("NereusSDR rendezvous id v1\n" || spki), its first 26
/// characters. Empty for anything that is not 91 bytes.
QString rendezvousId(const QByteArray& spki);
/// Section 4.2's form: exactly 26 characters of `a-z` and `2-7`.
bool isRendezvousId(const QString& text);
/// Section 4.3: "NereusSDR rendezvous register v1\n" || the challenge's 32
/// raw bytes.
QByteArray registerTranscript(const QByteArray& nonce);
/// Section 4.4: "NereusSDR introduce v1\n" || the station id's 26 ASCII
/// bytes || the introducing connection's hello nonce, 32 raw bytes.
QByteArray introduceTranscript(const QString& stationId, const QByteArray& nonce);

/// Section 5.2's candidate form: empty (the end of candidates), or starting
/// `candidate:`, at most kMaxCandidateBytes.
bool isCandidate(const QString& candidate);
/// A candidate as an ICE library gives it, ready for the wire: an SDP
/// line's `a=` removed (section 5.2), nothing else changed.
QString wireCandidate(const QString& fromIceLibrary);

} // namespace NereusSDR::RendezvousWire

Q_DECLARE_METATYPE(NereusSDR::RendezvousWire::Turn)
