#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/StationIdentity.h  (NereusSDR)
// =================================================================
//
// The Core's own identity key (iPhone app plan Task 12, R-IOS-08; the
// pairing design, docs/architecture/2026-08-02-remote-station-identity-
// and-pairing-design.md section 3.1, "The key is the station").
//
// One ECDSA P-256 key pair, created on the first start and kept in
// `station-identity.pem` (PKCS#8, PEM) in the Core's profile directory,
// mode 0600, written atomically. It is NOT the TLS key: CertificateStore's
// RSA key stays what it is, and this key signs a binding to that
// certificate instead ("NereusSDR cert-binding v1\n" || SHA-256 of the
// certificate DER), so a device that holds this key's public half can
// tell the certificate it was shown really belongs to this Core.
//
// Wire encodings (the link document, section 3.4):
//   - a public key travels as base64url, no padding, of its
//     SubjectPublicKeyInfo DER (91 bytes for P-256, uncompressed point);
//   - a signature is ECDSA P-256 over SHA-256, raw r || s, 64 bytes;
//   - a key's fingerprint is SHA-256 of its SubjectPublicKeyInfo DER.
//
// The private key never leaves the profile directory and is never logged.
// A key file that exists but cannot be read is never regenerated over:
// that would make every paired device pair again (section 3.2), so the
// identity is refused instead, as CertificateStore and TokenStore do.
//
// verify() is also what checks a DEVICE's signature, so it accepts only a
// P-256 key in its canonical 91-byte encoding: anything else is refused
// rather than interpreted.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: iPhone app Task 18 (R-IOS-08): loadOrCreateKeyFile(), so
//               the desktop's device key shares this handling. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
// =================================================================

#include <QByteArray>
#include <QString>

#include <memory>

struct evp_pkey_st;

namespace NereusSDR {

class StationIdentity {
public:
    /// The key file's name inside the profile directory.
    static constexpr const char* kKeyFileName = "station-identity.pem";
    /// SubjectPublicKeyInfo DER of a P-256 key with an uncompressed point.
    static constexpr int kSpkiBytes = 91;
    /// Raw r || s.
    static constexpr int kSignatureBytes = 64;

    /// An invalid identity (no key). loadOrCreate() is how one is made.
    StationIdentity();

    /// Loads `profileDir`/station-identity.pem, or creates it (mode 0600,
    /// atomically) when it does not exist. A file that exists but does not
    /// hold a P-256 private key gives an invalid identity whose lastError()
    /// says why; the file is left as it is.
    static StationIdentity loadOrCreate(const QString& profileDir);

    /// The same key handling for another key file in `profileDir`
    /// (iPhone app Task 18: the desktop's own device key,
    /// ClientDeviceIdentity). `whose` begins the log text ("The Core's",
    /// "This computer's").
    static StationIdentity loadOrCreateKeyFile(const QString& profileDir,
                                               const QString& fileName,
                                               const QString& whose);

    /// Read an existing P-256 private key only. Never creates directories/files,
    /// changes permissions, overwrites or regenerates a missing/invalid key.
    static StationIdentity loadExistingKeyFile(const QString& profileDir,
                                              const QString& fileName, const QString& whose);

    /// Part C fix wave (R1-M5): an existing secret file (a key, the paired
    /// devices) that other users can read or write, restored from a backup
    /// at 0644 say, is set back to mode 0600 when it is loaded; a warning
    /// is logged when that fails. True when the file is owner-only
    /// afterwards (or absent). Not on Windows, whose files carry no such
    /// mode (the profile directory's ACL protects them).
    static bool keepOwnerOnly(const QString& path);

    bool isValid() const { return m_key != nullptr; }
    QString lastError() const { return m_lastError; }
    /// True only on the start that created the key file (the first-run
    /// backup prompt goes out then).
    bool wasCreatedThisRun() const { return m_createdThisRun; }

    /// SubjectPublicKeyInfo DER. Empty when invalid.
    QByteArray publicKeySpki() const { return m_spki; }
    /// SHA-256 of publicKeySpki(), 32 bytes. Empty when invalid.
    QByteArray fingerprint() const;
    /// ECDSA P-256 over SHA-256 of `message`, raw r || s (64 bytes).
    /// Empty when invalid or when signing fails.
    QByteArray sign(const QByteArray& message) const;
    /// True when `signature` (raw r || s, 64 bytes) is `spki`'s signature
    /// over `message`. `spki` must be a P-256 key in its canonical 91-byte
    /// SubjectPublicKeyInfo DER.
    static bool verify(const QByteArray& spki, const QByteArray& message,
                       const QByteArray& signature);
    /// Where the key lives (populated even when loading failed).
    QString keyPath() const { return m_keyPath; }

    // ---- The link's identity values (the link document, section 3.4) ----

    /// "NereusSDR cert-binding v1\n" || certSha256.
    static QByteArray certBindingMessage(const QByteArray& certSha256);
    /// sign(certBindingMessage(certSha256)).
    QByteArray certBinding(const QByteArray& certSha256) const;
    /// SHA-256 of `spki`.
    static QByteArray fingerprintOf(const QByteArray& spki);
    /// True when `spki` is a P-256 public key in its canonical 91-byte DER.
    static bool isP256Spki(const QByteArray& spki);

    /// base64url without padding, the link's encoding for keys, signatures,
    /// fingerprints and challenges.
    static QString toBase64Url(const QByteArray& bytes);
    /// Strict: only the base64url alphabet, no padding, and a length that
    /// decodes. Sets *ok (when given) to whether it did.
    static QByteArray fromBase64Url(const QString& text, bool* ok = nullptr);

private:
    std::shared_ptr<evp_pkey_st> m_key;
    QByteArray m_spki;
    QString m_keyPath;
    QString m_lastError;
    bool m_createdThisRun = false;
};

} // namespace NereusSDR
