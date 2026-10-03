#pragma once
// =================================================================
// src/core/security/CertificateStore.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Remote Daemon R2, Task 17.
//
// Design source: docs/architecture/2026-07-28-remote-daemon-architecture-
// design.md §10.5 "Encryption, and which phase owns it":
//
//   "Certificate model. A headless Pi has no domain name, so the daemon
//   generates a self-signed certificate on first run and the client pins
//   its fingerprint, displayed at pairing time alongside the token
//   (§7.1)."
//
// Both remote-daemon design documents were searched (2026-08-08) for a
// key algorithm, key size, validity period, or fingerprint display
// format, and neither specifies any of the four -- see
// docs/architecture/2026-08-02-remote-station-identity-and-pairing-
// design.md §10.2, which lists "Certificate handling" as a component and
// cross-references it back to "already parent R2" (this document)
// without further detail, and its own §13 open item 10 ("Default
// rendezvous hostname, certificate strategy, and operational ownership.
// Not a code question, but it blocks packaging") which confirms the gap
// is known and still open. This task's choices, recorded once here
// rather than scattered across the .cpp:
//
//   - RSA 3072 / SHA-256, not ECDSA. Larger and slower to generate than
//     RSA 2048 or an EC curve, but this runs once per daemon lifetime
//     (a fresh key pair costs a few seconds, not a hot-path cost), and
//     RSA has no curve-selection dimension to get wrong against an
//     unknown-until-runtime Qt TLS backend (see tlsBackendDiagnostic()).
//     Fix round 1 review: 3072 replaces an initial choice of 2048 after
//     the reviewer flagged that 2048-bit RSA paired with this class's
//     10-year validity put the key's ~112-bit security strength (NIST-
//     rated through roughly 2030) on a certificate meant to still be in
//     service through roughly 2036, with no rotation mechanism to
//     revisit either number before then. 3072-bit RSA's strength margin
//     comfortably covers the 10-year horizon instead.
//   - 10-year validity (see generateAndStore() in the .cpp). No renewal
//     mechanism exists yet, and because the client PINS this
//     certificate's fingerprint (parent design §10.5), regenerating it
//     later (expiry, or an operator wiping the profile) breaks every
//     already-paired client until they re-pair. A long runway is a
//     deliberate hedge against that, not a fix for the missing rotation
//     story -- whichever task owns pairing/re-pairing should decide the
//     real one.
//   - Fingerprint display: colon-separated uppercase SHA-256 hex pairs
//     (see fingerprintSha256() below), the conventional X.509 form. This
//     task's own choice in the documented absence of a specified one.
//   - X.509v3 declared, zero v3 extensions. Deliberate, not an
//     oversight: under fingerprint pinning (the whole point of the
//     parent design's certificate model) extensions buy nothing today,
//     and adding them retroactively would change the DER encoding under
//     every already-paired client's pinned fingerprint, forcing a
//     re-pair. If a later task needs Qt's addCaCertificate() trust-chain
//     route instead of pinning, that is a deliberate, separate decision
//     to make then -- not something to grow into by accident here.
//
// SCOPE BOUNDARY: this class provisions a certificate and a private key,
// and nothing else. No socket, no handshake, no token -- those belong to
// Task 18 (the wss session) and its TokenStore. If you came here to add
// transport code, it belongs somewhere else.
//
// NOT THE SAME THING AS: the identity/pairing design doc's §2-3 describe
// a separate "asymmetric key pair... generated on first run, held by the
// machine" that is the station's identity for pairing and for deriving
// the rendezvous's scrambled registration name. That is a different
// artifact from the TLS certificate provisioned here, even though both
// are "a key pair generated on first run" in the abstract. Do not assume
// this class's RSA key can stand in for that identity key without an
// explicit decision to reuse it -- the two design sections were scoped
// apart on purpose (identity doc §10.2's "Certificate handling, already
// parent R2" cross-reference is what draws the line).
//
// AI tooling: Anthropic Claude Code.
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QSsl>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>

namespace NereusSDR {

// Generates (first run) or loads (every run after) a self-signed TLS
// identity for nereusd's wss:// listener: one RSA-3072 key pair and one
// X.509 certificate, PEM-encoded, stored beside the daemon profile's own
// settings file.
//
// Construction does all the work, synchronously -- matching this
// codebase's other small provisioning-on-construct core classes (see
// FaultLog::FaultLog, which loads its ring buffer the same way in its
// constructor). There is no separate "ensure" call to remember to invoke;
// check isValid() right after constructing.
class CertificateStore {
public:
    // The key algorithm every certificate/key this class produces or
    // accepts uses. A public constant (rather than a magic QSsl::Rsa
    // scattered at every call site) so callers that need to construct
    // their own QSslKey from this store's PEM files -- Task 18, or this
    // class's own tests -- stay in sync with whatever this task chose.
    static constexpr QSsl::KeyAlgorithm kKeyAlgorithm = QSsl::Rsa;

    // directory is where cert.pem / key.pem live; it is created
    // (mkpath, recursively) if it does not exist yet. Defaults to the
    // daemon profile's own config directory -- see defaultDirectory().
    // Tests pass an explicit scratch directory (a QTemporaryDir path) so
    // a test run never touches, or reads back, a real profile's TLS
    // identity.
    explicit CertificateStore(const QString& directory = defaultDirectory());

    // Where the production TLS identity lives: the config directory of the
    // profile this process is actually running under, resolved through
    // AppSettings::resolveConfigDir() rather than rebuilt by hand
    // (task-17-controller-notes.md).
    //
    // Follows AppSettings::profileOverride(), NOT a hardcoded
    // kDaemonProfileName. Hardcoding it meant `nereusd --profile alpha` and
    // `nereusd --profile beta` isolated their settings and their logs but
    // SHARED one certificate and one token, which defeats the point of
    // --profile: it exists to isolate instances on one workstation. Two
    // simultaneous first runs each minted a token and raced the rename, and
    // the loser then accepted a token that was not the one on disk.
    //
    // The default daemon run is unaffected: server_main.cpp resolves an
    // absent --profile to kDaemonProfileName (DaemonConfig.cpp's
    // resolveDaemonProfileArgument) and calls setProfileOverride() with it
    // before anything reaches here, so profileOverride() is already
    // "daemon" and this resolves to exactly the directory it always did. No
    // migration needed. An explicit `--profile ""`, the documented escape
    // hatch back to the shared directory, now shares the security material
    // too, which is the consistent reading of what the operator asked for.
    //
    // A free function so a caller can read where the real one lives without
    // constructing a store.
    static QString defaultDirectory();

    // True once a certificate and private key are loaded and usable.
    // False means lastError() names why: OpenSSL key/certificate
    // generation failed, the on-disk files exist but do not parse as
    // either, or Qt itself reports no working TLS backend (see
    // tlsBackendDiagnostic(), whose message becomes part of lastError()
    // in that case).
    bool isValid() const { return m_valid; }

    // Empty when isValid() is true.
    QString lastError() const { return m_lastError; }

    // Empty/null when isValid() is false.
    QSslCertificate certificate() const { return m_certificate; }
    QSslKey privateKey() const { return m_privateKey; }

    // Absolute paths to the PEM files this instance will load from or
    // write to. Fix round 1 review minor 6: always populated, derived
    // from the constructor's directory argument unconditionally in the
    // member-initializer list (see the .cpp) before any provisioning is
    // attempted -- never empty, regardless of isValid().
    QString certificatePath() const { return m_certPath; }
    QString privateKeyPath() const { return m_keyPath; }

    // SHA-256 digest of the certificate's DER encoding, formatted as 32
    // colon-separated uppercase hex byte pairs (e.g.
    // "AB:12:CD:...:FF") -- what `openssl x509 -fingerprint -sha256`
    // prints, minus its "SHA256 Fingerprint=" label. See the top-of-file
    // note: neither design document specifies a display format for the
    // "TLS fingerprint, displayed at pairing time" (parent design
    // §7.1/§10.5); this is this task's chosen default.
    //
    // Computed directly from the OpenSSL X509* at generation/load time
    // (X509_digest(), not QSslCertificate::digest()), so it stays
    // available even when tlsBackendDiagnostic() reports a degraded Qt
    // TLS backend -- Task 18 wants something printable to qCInfo on
    // first run regardless of whether the backend can actually complete
    // a handshake yet. Empty when isValid() is false.
    QString fingerprintSha256() const { return m_fingerprint; }

    // Empty when Qt has a working TLS backend (QSslSocket::supportsSsl()
    // == true). Otherwise names the cause as specifically as this
    // platform allows -- in particular, a Windows Qt build whose only
    // compiled-in backend is Schannel, called out explicitly because a
    // generic "TLS unavailable" message costs a maintainer real
    // debugging time (task-17-brief.md Step 4). Static and independent
    // of any instance: this is a property of the Qt build in use, not of
    // a particular certificate, so it is callable without provisioning
    // anything.
    static QString tlsBackendDiagnostic();

private:
    // Fix round 1, Important 3: loadExisting() used to collapse three
    // distinct outcomes into a bool, which is exactly what let "exists
    // but this process cannot read it" get treated the same as "does not
    // exist yet" and silently regenerated over. Loaded and NotPresent
    // both existed before (as true/false); IoFailure is new and is the
    // one outcome the constructor must NOT respond to by calling
    // generateAndStore() -- doing so would overwrite a station identity
    // the operator cannot currently prove is wrong, which is worse than
    // refusing to start.
    enum class LoadResult {
        Loaded,      // a matching cert + key pair was parsed successfully
        NotPresent,  // missing, corrupt, or a mismatched pair -- safe to
                     // regenerate, exactly like a genuine first run
        IoFailure,   // exists but this process could not open it -- must
                     // NOT be regenerated over; see m_lastError
    };

    // See LoadResult above for the three outcomes.
    LoadResult loadExisting();

    // Creates a fresh RSA-3072 key pair and self-signed certificate,
    // writes both to m_certPath / m_keyPath (restrictive permissions on
    // the key), and populates m_certificate / m_privateKey /
    // m_fingerprint from the freshly generated bytes. Returns false (and
    // sets m_lastError) on any OpenSSL failure.
    bool generateAndStore();

    QString m_directory;
    QString m_certPath;
    QString m_keyPath;
    bool    m_valid{false};
    QString m_lastError;
    QSslCertificate m_certificate;
    QSslKey         m_privateKey;
    QString m_fingerprint;
};

} // namespace NereusSDR
