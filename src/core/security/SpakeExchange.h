#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/SpakeExchange.h  (NereusSDR)
// =================================================================
//
// The pairing code's key exchange (iPhone app plan Task 14, R-IOS-08,
// spec D37; the pairing design,
// docs/architecture/2026-08-02-remote-station-identity-and-pairing-design.md
// section 4.3): SPAKE2+EE (jedisct1/spake2-ee) on libsodium, both pinned in
// cmake/NereusPairing.cmake. Nothing else in NereusSDR includes their
// headers.
//
// A device and the Core that share the short code agree two keys, and
// each side learns whether the other held the same code. Whoever carries
// the messages (the rendezvous, Part E) learns nothing it could test
// guesses against offline: each exchange is one online guess, and the
// Core burns the code after it (PairingWindow).
//
// The steps, as spake2-ee numbers them and the link carries them
// (`pair.spake`):
//
//   0  Core -> device   the password hash parameters and salt (36 bytes)
//   1  device -> Core   the device's share (32 bytes); the device checks
//                       step 0's parameters are exactly the fixed ones
//                       before it hashes the code
//   2  Core -> device   the Core's share and the device's validator
//                       (64 bytes); the Core has now committed to the code
//   3  device -> Core   the Core's validator (32 bytes); the device fails
//                       here when the codes differ
//   4  (the Core)       checks step 3; the Core fails here when they differ
//
// Fixed on both sides (the plan's Part C wire values): client identity
// "nereussdr-device-v1", server identity "nereussdr-station-v1", password
// hashing crypto_pwhash_OPSLIMIT_INTERACTIVE and
// crypto_pwhash_MEMLIMIT_INTERACTIVE with libsodium's default algorithm
// (Argon2id). The password is the normalised code (PairingCode::normalise)
// as UTF-8.
//
// Then the confirmation boxes (`pair.confirm`): XChaCha20-Poly1305 (IETF)
// under the shared keys, the device's box with client_sk and the Core's
// with server_sk, each a fresh random 24-byte nonce followed by the
// ciphertext, no additional data.
//
// Secret material (the stored data derived from the code, the states, the
// shared keys) is wiped when it is replaced and when this object goes. It
// is never logged.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QByteArray>
#include <QString>

#include <memory>
#include <optional>

namespace NereusSDR {

class SpakeExchange {
public:
    enum class Role { Station, Device };

    static constexpr const char* kClientId = "nereussdr-device-v1";
    static constexpr const char* kServerId = "nereussdr-station-v1";
    static constexpr int kPublicDataBytes = 36;   // step 0
    static constexpr int kResponse1Bytes = 32;    // step 1
    static constexpr int kResponse2Bytes = 64;    // step 2
    static constexpr int kResponse3Bytes = 32;    // step 3
    static constexpr int kStoredBytes = 164;
    static constexpr int kNonceBytes = 24;
    static constexpr int kTagBytes = 16;

    /// libsodium initialised and usable. Everything below fails closed when
    /// it is not.
    static bool isAvailable();

    /// What the Core keeps for one code: the password hash of
    /// `normalisedCode` with a fresh salt (crypto_spake_server_store, the
    /// fixed parameters). Empty on failure. Costs one Argon2id hash.
    static QByteArray storedData(const QString& normalisedCode);
    /// Overwrites `bytes` with zeros (sodium_memzero) and empties it: for
    /// stored data and anything else derived from a code, when it is done.
    static void wipe(QByteArray& bytes);

    explicit SpakeExchange(Role role);
    ~SpakeExchange();
    SpakeExchange(const SpakeExchange&) = delete;
    SpakeExchange& operator=(const SpakeExchange&) = delete;

    Role role() const { return m_role; }

    // ── The Core (Role::Station) ──────────────────────────────────────

    /// Step 0 from `stored` (storedData()). Empty on failure.
    QByteArray stationStep0(const QByteArray& stored);
    /// Step 2, answering the device's step 1. Empty on failure (a malformed
    /// share, or step 0 not taken).
    QByteArray stationStep2(const QByteArray& stored, const QByteArray& response1);
    /// Step 4: true when the device's step 3 shows it held the same code;
    /// the shared keys are then set.
    bool stationStep4(const QByteArray& response3);

    // ── The device (Role::Device) ─────────────────────────────────────

    /// Step 1 from the Core's step 0 and the normalised code. Empty when
    /// step 0 is malformed or names other hash parameters than the fixed
    /// ones (a hostile peer choosing a weak hash), or on failure.
    QByteArray deviceStep1(const QByteArray& publicData, const QString& normalisedCode);
    /// Step 3 from the Core's step 2. Empty when the Core did not hold the
    /// same code (or step 2 is malformed); the shared keys are set
    /// otherwise.
    QByteArray deviceStep3(const QByteArray& response2);

    // ── Both ──────────────────────────────────────────────────────────

    /// The shared keys are agreed (station step 4, device step 3).
    bool isComplete() const;
    /// This side's confirmation box around `plaintext`: the device's with
    /// client_sk, the Core's with server_sk. Empty before isComplete().
    QByteArray sealConfirmation(const QByteArray& plaintext) const;
    /// The other side's box opened: the Core opens the device's with
    /// client_sk, the device the Core's with server_sk. nullopt when it
    /// does not open (tampered, the wrong key, malformed).
    std::optional<QByteArray> openConfirmation(const QByteArray& box) const;

private:
    struct Secrets;
    Role m_role;
    std::unique_ptr<Secrets> m_secrets;
};

} // namespace NereusSDR
