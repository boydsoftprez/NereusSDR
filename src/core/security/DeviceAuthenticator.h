#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/DeviceAuthenticator.h  (NereusSDR)
// =================================================================
//
// Device sign-in (iPhone app plan Task 12, R-IOS-08, R-IOS-02; the link
// document, docs/architecture/2026-09-23-station-link-v1.md section 3.5).
//
// Each connection gets a fresh 32-byte challenge from the operating
// system's generator, sent in the Core's `hello`. A paired device signs
// the transcript
//
//   "NereusSDR device-auth v1\n" || challenge (32 bytes)
//     || SHA-256(TLS certificate DER) || SHA-256(Core identity SPKI DER)
//     || SHA-256(device SPKI DER)
//
// with its own P-256 key and sends the signature in `auth.request`'s
// `device` block. The Core admits it only when the device is in its
// DeviceStore with that very key and the signature verifies over THIS
// connection's challenge and THIS Core's certificate. A signature over
// another connection's challenge, or one that binds another certificate
// (a device that was shown someone else's certificate on the way in),
// does not verify, so it is refused like any wrong signature.
//
// ---- Rate limits ----
//
// Failures are counted per source address and per device id: 10 within
// 60 s refuse that address, or that id, for 60 s (kMaxFailures,
// kWindowMs, kLockoutMs). A failed proof counts against the address and
// introduction only, never the id: it did not come from the id's key, so
// counting it there would let anyone who knows a paired device's id lock
// that device out. Only a proved key that is not paired counts against
// its id. Connections through the relay share the relay's address, so
// there the address is empty and the limit applies per introduction (and
// per id, for proved keys) instead. This limiter is the device path's
// own: it never consults the pairing token's limiter or (later) the
// pairing-code limiter, and those never consult it, so token guesses
// cannot lock out a device key and the reverse. A refused attempt while
// limited is not counted again. The clock is injected; tests never sleep.
//
// The authenticator holds no secret; it reads the Core's public key and
// the paired devices, and verifies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: LINK minor 4: a failed proof no longer counts against the
//               device id it names. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QString>

#include <functional>
#include <optional>

#include "core/security/DeviceStore.h"

namespace NereusSDR {

class StationIdentity;

/// `auth.request`'s `device` block as it arrived, plus where it came from.
struct DeviceAuthRequest {
    QString id;          // base64url of the device key's fingerprint
    QString publicKey;   // base64url of the device key's SPKI DER
    QString name;
    QString kind;
    QString signature;   // base64url of raw r || s
    /// The peer's address; empty over the relay.
    QString sourceAddress;
    /// The relay's introduction this connection came through; empty on a
    /// direct connection.
    QString introduction;
};

struct AuthOutcome {
    enum class Result {
        Admitted,        // a paired device, proved
        Proved,          // verifyPossession(): the key signed this transcript
        NotPaired,       // a well-formed proof from a key the Core does not know
        ProofFailed,     // malformed block, key and id disagree, bad signature
        RateLimited,     // not checked: this address, id or introduction is refused for now
    };
    Result result = Result::ProofFailed;
    /// The device's id (raw fingerprint) when the block named a readable
    /// one.
    QByteArray deviceId;
    /// The key and details, decoded, when the proof held (Admitted,
    /// Proved, NotPaired).
    QByteArray publicKeySpki;

    bool admitted() const { return result == Result::Admitted; }
};

class DeviceAuthenticator {
public:
    static constexpr int kChallengeBytes = 32;
    static constexpr int kMaxFailures = 10;
    static constexpr qint64 kWindowMs = 60000;
    static constexpr qint64 kLockoutMs = 60000;
    /// How many addresses, ids and introductions the limiter remembers.
    static constexpr int kMaxTrackedKeys = 4096;

    using Clock = std::function<qint64()>;

    /// `store` and `identity` are not owned and must outlive this object.
    /// `clock` returns milliseconds on a monotonic scale; default: a steady
    /// clock.
    DeviceAuthenticator(const DeviceStore& store, const StationIdentity& identity,
                        Clock clock = {});

    /// 32 random bytes from the operating system's generator.
    QByteArray newChallenge();

    /// A paired device's sign-in: rate limited, then proof, then the store.
    AuthOutcome verify(const DeviceAuthRequest& request, const QByteArray& challenge,
                       const QByteArray& certSha256);

    /// The proof alone, for a window enrolling its key while it signs in
    /// with the pairing token: the key must sign this connection's
    /// transcript, whether or not it is paired. Result Proved or
    /// ProofFailed; not rate limited and not counted, since the token
    /// already vouched for the connection.
    AuthOutcome verifyPossession(const DeviceAuthRequest& request, const QByteArray& challenge,
                                 const QByteArray& certSha256) const;

    /// The transcript a device signs (see the header comment).
    static QByteArray transcript(const QByteArray& challenge, const QByteArray& certSha256,
                                 const QByteArray& stationSpki, const QByteArray& deviceSpki);

    /// True while `request`'s address, id or introduction is refused.
    bool isRateLimited(const DeviceAuthRequest& request) const;

private:
    struct Limit {
        QList<qint64> failures;
        qint64 refusedUntil = 0;
    };

    qint64 now() const;
    /// The keys a request is limited by; the id only when `countId`.
    QStringList limitKeys(const DeviceAuthRequest& request, bool countId) const;
    void recordFailure(const DeviceAuthRequest& request, bool countId);
    void prune(qint64 now);

    const DeviceStore& m_store;
    const StationIdentity& m_identity;
    Clock m_clock;
    QHash<QString, Limit> m_limits;
};

} // namespace NereusSDR
