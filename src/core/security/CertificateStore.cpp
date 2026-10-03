// =================================================================
// src/core/security/CertificateStore.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See CertificateStore.h for the full
// design rationale (key algorithm, validity period, and fingerprint
// format choices, all made in the documented absence of a design-doc
// specification for any of the three).
//
// AI tooling: Anthropic Claude Code.
//
// Modification history (NereusSDR):
//   2026-09-30: LINK minor 6: the private key is made owner-only again
//               on every load. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/security/CertificateStore.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QSaveFile>
#include <QSslSocket>

#include <memory>

namespace NereusSDR {

namespace {

// RAII wrappers over the OpenSSL C API. "No raw new/delete" (CLAUDE.md
// C++ style guide) applies here in spirit even though these are
// alloc/free function pairs rather than new/delete: without this, every
// early-return path below (and there are several -- each OpenSSL call
// can fail) would need its own manual cleanup, which is exactly the
// leak-on-early-return shape unique_ptr exists to remove.
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using X509Ptr    = std::unique_ptr<X509, decltype(&X509_free)>;
using BioPtr      = std::unique_ptr<BIO, decltype(&BIO_free)>;

EvpPkeyPtr makeEvpPkeyPtr(EVP_PKEY* p) { return EvpPkeyPtr(p, &EVP_PKEY_free); }
X509Ptr    makeX509Ptr(X509* p)        { return X509Ptr(p, &X509_free); }
BioPtr     makeBioPtr(BIO* p)          { return BioPtr(p, &BIO_free); }

// RSA key size in bits. NereusSDR-original choice (see CertificateStore.h
// top-of-file note): neither remote-daemon design document specifies an
// algorithm or size for this certificate. Fix round 1 review: 3072
// (not 2048) specifically because kValiditySeconds below is 10 years --
// 2048-bit RSA's ~112-bit security strength is NIST-rated through
// roughly 2030, short of this certificate's ~2036 horizon with no
// rotation mechanism to revisit either number before then. The
// generation cost difference (about a second versus a few) is paid once
// per daemon lifetime, not per connection.
constexpr int kRsaKeyBits = 3072;

// Certificate validity window. NereusSDR-original choice, see
// CertificateStore.h: no rotation mechanism exists yet, and the client
// pins this certificate's fingerprint, so a long runway is deliberate.
constexpr long kNotBeforeSkewSeconds = -60L * 60L * 24L;             // -1 day
constexpr long kValiditySeconds      = 60L * 60L * 24L * 365L * 10L; // 10 years

// Subject/issuer CN for the self-signed certificate. Not verified by
// anything -- the client pins the fingerprint instead (parent design
// §10.5) -- so this exists only to give the certificate a human-legible
// label if someone inspects it by hand (e.g. `openssl x509 -text`).
const char* const kSubjectCommonName = "nereusd";

QString drainOpenSslErrors()
{
    QStringList parts;
    unsigned long code;
    char buf[256];
    while ((code = ERR_get_error()) != 0) {
        ERR_error_string_n(code, buf, sizeof(buf));
        parts << QString::fromLatin1(buf);
    }
    if (parts.isEmpty()) {
        return QStringLiteral("unknown OpenSSL error");
    }
    return parts.join(QStringLiteral("; "));
}

// Formats a SHA-256 digest as 32 colon-separated uppercase hex byte
// pairs -- see CertificateStore.h's fingerprintSha256() doc comment for
// why this format and not another.
QString formatFingerprint(const unsigned char* digest, unsigned int len)
{
    QStringList bytes;
    bytes.reserve(static_cast<int>(len));
    for (unsigned int i = 0; i < len; ++i) {
        bytes << QStringLiteral("%1").arg(digest[i], 2, 16, QLatin1Char('0'))
                                      .toUpper();
    }
    return bytes.join(QLatin1Char(':'));
}

// Reads the SHA-256 fingerprint straight from an OpenSSL X509*, via
// X509_digest() -- not through QSslCertificate::digest(). See
// CertificateStore.h: this keeps the fingerprint available even when
// tlsBackendDiagnostic() reports a degraded Qt TLS backend.
QString fingerprintOf(X509* cert)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (X509_digest(cert, EVP_sha256(), digest, &len) != 1) {
        return QString();
    }
    return formatFingerprint(digest, len);
}

// Serializes an X509*/EVP_PKEY* pair to PEM bytes via in-memory BIOs, so
// the same bytes can be written to disk AND handed to QSslCertificate/
// QSslKey without a redundant round trip through the filesystem.
bool pemEncode(X509* cert, EVP_PKEY* pkey, QByteArray* certPemOut, QByteArray* keyPemOut)
{
    BioPtr certBio = makeBioPtr(BIO_new(BIO_s_mem()));
    BioPtr keyBio  = makeBioPtr(BIO_new(BIO_s_mem()));
    if (!certBio || !keyBio) {
        return false;
    }
    if (PEM_write_bio_X509(certBio.get(), cert) != 1) {
        return false;
    }
    // No passphrase: nereusd runs unattended (systemd unit, headless
    // boot), so an encrypted key would need a passphrase prompt this
    // process has nowhere to show. File permissions are the protection
    // mechanism instead (see writePemFile() below) -- the same trust
    // model most daemon TLS keys (sshd, nginx) use.
    if (PEM_write_bio_PrivateKey(keyBio.get(), pkey, nullptr, nullptr, 0,
                                  nullptr, nullptr) != 1) {
        return false;
    }

    char* certData = nullptr;
    const long certLen = BIO_get_mem_data(certBio.get(), &certData);
    char* keyData = nullptr;
    const long keyLen = BIO_get_mem_data(keyBio.get(), &keyData);
    if (certLen <= 0 || keyLen <= 0) {
        return false;
    }
    *certPemOut = QByteArray(certData, static_cast<int>(certLen));
    *keyPemOut  = QByteArray(keyData, static_cast<int>(keyLen));
    return true;
}

// Atomic write (QSaveFile, matching AppSettings::save()'s own pattern)
// plus, for the private key only, owner-only permissions. restrictToOwner
// is called unconditionally (not #ifdef Q_OS_UNIX guarded): on Windows
// QFileDevice::ReadOwner|WriteOwner is still a reasonable best-effort ACL
// restriction, it just cannot be verified back with QFile::permissions()
// the same way (see tests/tst_certificate_store.cpp's
// privateKeyFileIsOwnerOnlyOnUnix() for why the *test* assertion, not
// this call, is what is UNIX-only).
//
// Fix round 1 review minor 7: permissions are set on the QSaveFile's own
// temp file BEFORE commit(), not via a separate QFile::setPermissions()
// call on the final path AFTER commit() (a prior revision did the
// latter). QSaveFile::commit() atomically renames its temp file into
// place; rename() does not alter permission bits, so setting them on the
// still-open temp file means the tight permissions are already in effect
// for the entire time any file exists at all under the final name --
// there is no window where the private key sits at the temp file's
// default (often world-readable) permissions after being renamed into
// place but before a separate chmod call catches up. setPermissions()'s
// return value is checked and treated as fatal for the key (an
// unprotected private key is a real problem worth failing loudly over,
// not silently continuing past).
bool writePemFile(const QString& path, const QByteArray& pem, bool restrictToOwner)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (restrictToOwner) {
        if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            file.cancelWriting();
            return false;
        }
    }
    if (file.write(pem) != pem.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace

CertificateStore::CertificateStore(const QString& directory)
    : m_directory(directory)
    , m_certPath(directory + QStringLiteral("/tls-cert.pem"))
    , m_keyPath(directory + QStringLiteral("/tls-key.pem"))
{
    QDir().mkpath(m_directory);

    bool provisioned = false;
    switch (loadExisting()) {
    case LoadResult::Loaded:
        provisioned = true;
        break;
    case LoadResult::IoFailure:
        // m_lastError already set by loadExisting(). Deliberately NOT
        // falling through to generateAndStore() here -- that is exactly
        // the Important 3 fix (fix round 1 review): an existing file
        // this process cannot read must fail loudly, never be silently
        // regenerated over.
        provisioned = false;
        break;
    case LoadResult::NotPresent:
        provisioned = generateAndStore();
        break;
    }

    // Fix round 1 review minor 5: computed unconditionally, before
    // deciding whether to return early, rather than only on the success
    // path a prior revision took. A missing Qt TLS backend can be WHY
    // QSslCertificate/QSslKey parsing failed inside loadExisting() or
    // generateAndStore() above, and the operator deserves that as part
    // of the answer rather than just "provisioning failed". See
    // CertificateStore.h's tlsBackendDiagnostic() doc comment and
    // task-17-brief.md Step 4.
    const QString backendIssue = tlsBackendDiagnostic();

    if (!provisioned) {
        if (!backendIssue.isEmpty()) {
            m_lastError = QStringLiteral("%1 (TLS backend: %2)")
                              .arg(m_lastError, backendIssue);
        }
        return;
    }

    if (!backendIssue.isEmpty()) {
        m_valid = false;
        m_lastError = backendIssue;
        qCWarning(lcApp) << "CertificateStore:" << backendIssue;
    }
}

QString CertificateStore::defaultDirectory()
{
    // profileOverride() when there is one, kDaemonProfileName otherwise.
    // See the header for why this follows --profile at all.
    //
    // isEmpty(), not isNull(), and the fallback is deliberate rather than
    // defensive. server_main.cpp calls setProfileOverride() only for a
    // NON-empty resolved profile, so `nereusd --profile ""` -- the
    // documented escape hatch back to the shared settings directory --
    // leaves the override unset. Falling back to the reserved daemon
    // profile keeps the token and the private key out of the user's shared
    // config directory even then, which is the conservative choice for a
    // secret and is not what that escape hatch was asking for. It also
    // means any process that never sets an override still gets the
    // reserved directory rather than the shared one.
    const QString profile = AppSettings::profileOverride();
    return AppSettings::resolveConfigDir(
        profile.isEmpty() ? QString::fromLatin1(AppSettings::kDaemonProfileName)
                          : profile);
}

CertificateStore::LoadResult CertificateStore::loadExisting()
{
    QFile certFile(m_certPath);
    QFile keyFile(m_keyPath);
    if (!certFile.exists() || !keyFile.exists()) {
        // Genuinely missing -- never provisioned, or an operator/test
        // deleted one half. Safe to fall through to regeneration, same
        // as a first run.
        return LoadResult::NotPresent;
    }

    // Fix round 1, Important 3: both files EXIST from here on, so
    // failing to open one is an I/O failure (most realistically a
    // permission mismatch -- provisioned once by hand, then run under a
    // different systemd User=), not "no identity yet". This is the one
    // branch that must return IoFailure rather than NotPresent: falling
    // through to generateAndStore() here would silently overwrite a
    // pinned station identity every already-paired client depends on,
    // with no error and no log line.
    if (!certFile.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral(
            "Certificate file exists at %1 but could not be opened (%2); "
            "refusing to overwrite it. Check file permissions.")
            .arg(m_certPath, certFile.errorString());
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return LoadResult::IoFailure;
    }
    if (!keyFile.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral(
            "Private key file exists at %1 but could not be opened (%2); "
            "refusing to overwrite it. Check file permissions.")
            .arg(m_keyPath, keyFile.errorString());
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return LoadResult::IoFailure;
    }

    const QByteArray certPem = certFile.readAll();
    const QByteArray keyPem  = keyFile.readAll();

    BioPtr certBio = makeBioPtr(
        BIO_new_mem_buf(certPem.constData(), static_cast<int>(certPem.size())));
    BioPtr keyBio  = makeBioPtr(
        BIO_new_mem_buf(keyPem.constData(), static_cast<int>(keyPem.size())));
    if (!certBio || !keyBio) {
        ERR_clear_error();
        return LoadResult::NotPresent;
    }

    X509Ptr cert = makeX509Ptr(PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr));
    EvpPkeyPtr pkey = makeEvpPkeyPtr(
        PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr));
    if (!cert || !pkey) {
        // Corrupt or foreign-format file content. Not an I/O failure --
        // this process COULD read the bytes, they just are not a valid
        // cert/key -- so fall through to regeneration, same as a first
        // run (see the class-level doc comment and
        // tst_certificate_store.cpp's regeneratesWhenStoredFilesAreCorrupt()).
        ERR_clear_error();
        return LoadResult::NotPresent;
    }

    // Fix round 1, Important 2: each half can be individually
    // well-formed PEM and still not belong together -- exactly what a
    // write that fails between the certificate write and the key write
    // in generateAndStore() (ENOSPC on an SD card being the realistic
    // case) leaves on disk: a NEW certificate beside the OLD key, or the
    // reverse. Reject a mismatched pair the same way corrupt content is
    // rejected -- fall through to regeneration -- rather than handing
    // Task 18 a certificate that does not match the key beside it, which
    // would fail every client's TLS handshake with no clue why.
    if (X509_check_private_key(cert.get(), pkey.get()) != 1) {
        // Fix round 3 minor: this branch was silent, leaving the operator
        // with only generateAndStore()'s later "generated new TLS
        // identity" line and no statement of why the pinned identity is
        // about to be rotated -- unlike the two IoFailure branches above,
        // which both explain themselves. A mismatched pair rotating the
        // identity every already-paired client depends on deserves the
        // same visibility.
        qCWarning(lcApp) << "CertificateStore: certificate/key pair at"
                          << m_certPath << "and" << m_keyPath
                          << "do not match (X509_check_private_key failed); "
                             "regenerating a fresh identity.";
        ERR_clear_error();
        return LoadResult::NotPresent;
    }

    const QString fingerprint = fingerprintOf(cert.get());
    if (fingerprint.isEmpty()) {
        ERR_clear_error();
        return LoadResult::NotPresent;
    }

    m_certificate = QSslCertificate(certPem, QSsl::Pem);
    m_privateKey  = QSslKey(keyPem, kKeyAlgorithm, QSsl::Pem, QSsl::PrivateKey);
    if (m_certificate.isNull() || m_privateKey.isNull()) {
        ERR_clear_error();
        return LoadResult::NotPresent;
    }

    // LINK minor 6: a key file loosened since it was written (a restore
    // from backup, a copy, a hand edit) is made owner-only again on every
    // load, as generateAndStore() writes it. Best effort, as there: a
    // failure is logged and the identity still loads.
    keyFile.close();
    if (!QFile::setPermissions(m_keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        qCWarning(lcApp) << "CertificateStore: could not make the private key at" << m_keyPath
                         << "owner-only";
    }

    m_fingerprint = fingerprint;
    m_valid = true;
    m_lastError.clear();
    return LoadResult::Loaded;
}

bool CertificateStore::generateAndStore()
{
    // Fix round 1 review minor 8: every OpenSSL setter below now checks
    // its return value. A prior revision left X509_set_version(),
    // ASN1_INTEGER_set_int64(), the two X509_gmtime_adj() calls,
    // X509_set_pubkey(), X509_NAME_add_entry_by_txt() and
    // X509_set_issuer_name() unchecked; this local helper drains and
    // reports the OpenSSL error queue the same way every existing
    // checked call in this function already did, so each new check is
    // one line instead of a repeated four-line block.
    auto failOpenSsl = [this](const char* what) -> bool {
        m_lastError = QStringLiteral("%1 failed: %2")
                          .arg(QString::fromLatin1(what), drainOpenSslErrors());
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return false;
    };

    // OpenSSL 3.0's one-call key generator. See CertificateStore.h's
    // top-of-file note for why RSA over ECDSA, and the find_package(
    // OpenSSL 3.0 ...) block in the top-level CMakeLists.txt for the
    // version floor this call depends on.
    EvpPkeyPtr pkey = makeEvpPkeyPtr(EVP_RSA_gen(kRsaKeyBits));
    if (!pkey) {
        return failOpenSsl("RSA key generation");
    }

    X509Ptr cert = makeX509Ptr(X509_new());
    if (!cert) {
        return failOpenSsl("X.509 certificate allocation");
    }

    if (X509_set_version(cert.get(), 2L) != 1) { // X.509v3 (0-indexed: 2 == v3)
        return failOpenSsl("Setting certificate version");
    }

    // Random positive 63-bit serial (RAND_bytes then clear the sign bit,
    // so ASN1_INTEGER_set_int64 always encodes a positive INTEGER).
    unsigned char serialBytes[8];
    if (RAND_bytes(serialBytes, sizeof(serialBytes)) != 1) {
        return failOpenSsl("Random serial generation");
    }
    serialBytes[0] &= 0x7Fu;
    int64_t serial = 0;
    for (unsigned char b : serialBytes) {
        serial = (serial << 8) | static_cast<int64_t>(b);
    }
    if (ASN1_INTEGER_set_int64(X509_get_serialNumber(cert.get()), serial) != 1) {
        return failOpenSsl("Setting certificate serial number");
    }

    if (X509_gmtime_adj(X509_getm_notBefore(cert.get()), kNotBeforeSkewSeconds) == nullptr) {
        return failOpenSsl("Setting certificate notBefore");
    }
    if (X509_gmtime_adj(X509_getm_notAfter(cert.get()), kValiditySeconds) == nullptr) {
        return failOpenSsl("Setting certificate notAfter");
    }

    if (X509_set_pubkey(cert.get(), pkey.get()) != 1) {
        return failOpenSsl("Setting certificate public key");
    }

    // X.509v3, zero v3 extensions: a deliberate decision, not an
    // oversight -- see CertificateStore.h's top-of-file note (review
    // minor 9). Under fingerprint pinning, extensions buy nothing today,
    // and adding them later would change the DER encoding under every
    // already-paired client's pinned fingerprint.
    X509_NAME* name = X509_get_subject_name(cert.get());
    if (X509_NAME_add_entry_by_txt(
            name, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>(kSubjectCommonName), -1, -1, 0)
        != 1) {
        return failOpenSsl("Setting certificate subject name");
    }
    // Self-signed: issuer == subject. X509_get_subject_name() above
    // returns a pointer to the certificate's OWN internal name field, so
    // a prior revision's X509_set_subject_name(cert.get(), name) call
    // here was re-setting that field to itself -- a no-op at best, and a
    // use-after-free footgun on OpenSSL 1.0.x (only made safe by a
    // self-assignment guard OpenSSL >= 1.1 added). Deleted rather than
    // kept as a defensive-looking no-op (review minor 8).
    if (X509_set_issuer_name(cert.get(), name) != 1) {
        return failOpenSsl("Setting certificate issuer name");
    }

    if (X509_sign(cert.get(), pkey.get(), EVP_sha256()) == 0) {
        return failOpenSsl("Certificate signing");
    }

    QByteArray certPem;
    QByteArray keyPem;
    if (!pemEncode(cert.get(), pkey.get(), &certPem, &keyPem)) {
        m_lastError = QStringLiteral("PEM encoding failed: %1")
                          .arg(drainOpenSslErrors());
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return false;
    }

    if (!writePemFile(m_certPath, certPem, /*restrictToOwner=*/false)) {
        m_lastError = QStringLiteral("Could not write certificate to %1")
                          .arg(m_certPath);
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return false;
    }
    if (!writePemFile(m_keyPath, keyPem, /*restrictToOwner=*/true)) {
        m_lastError = QStringLiteral("Could not write private key to %1")
                          .arg(m_keyPath);
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return false;
    }

    const QString fingerprint = fingerprintOf(cert.get());
    if (fingerprint.isEmpty()) {
        m_lastError = QStringLiteral("Fingerprint computation failed: %1")
                          .arg(drainOpenSslErrors());
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return false;
    }

    m_certificate = QSslCertificate(certPem, QSsl::Pem);
    m_privateKey  = QSslKey(keyPem, kKeyAlgorithm, QSsl::Pem, QSsl::PrivateKey);
    if (m_certificate.isNull() || m_privateKey.isNull()) {
        m_lastError = QStringLiteral(
            "Generated certificate/key did not parse back through Qt "
            "(QSslCertificate/QSslKey); see tlsBackendDiagnostic().");
        qCWarning(lcApp) << "CertificateStore:" << m_lastError;
        return false;
    }

    m_fingerprint = fingerprint;
    m_valid = true;
    m_lastError.clear();
    qCInfo(lcApp) << "CertificateStore: generated new TLS identity at" << m_directory;
    return true;
}

// static
QString CertificateStore::tlsBackendDiagnostic()
{
    if (QSslSocket::supportsSsl()) {
        return QString();
    }

    const QString active = QSslSocket::activeBackend();
    const QStringList available = QSslSocket::availableBackends();
    const QString activeDisplay = active.isEmpty() ? QStringLiteral("<none>") : active;
    const QString availableDisplay =
        available.isEmpty() ? QStringLiteral("<none>") : available.join(QStringLiteral(", "));

#ifdef Q_OS_WIN
    // task-17-brief.md Step 4 / task-17-controller-notes.md: Qt on
    // Windows can be built with Schannel as its only TLS backend, which
    // reports QSslSocket::supportsSsl() == false when it cannot complete
    // its own runtime capability check. Named explicitly so this reads
    // as an actionable diagnosis rather than "TLS unavailable".
    if (!available.contains(QStringLiteral("openssl"))) {
        return QStringLiteral(
                   "Qt reports no usable TLS backend (active: %1; available: "
                   "%2). This Qt build was not compiled with the \"openssl\" "
                   "TLS backend, which is the likely cause on Windows -- Qt's "
                   "Schannel backend alone does not satisfy nereusd's "
                   "self-signed-certificate TLS requirements. Rebuild or "
                   "reinstall Qt with the OpenSSL TLS backend enabled, or "
                   "ship OpenSSL's DLLs alongside nereusd if the \"openssl\" "
                   "backend is present but its libraries are not found.")
            .arg(activeDisplay, availableDisplay);
    }
#endif

    return QStringLiteral(
               "Qt reports no usable TLS backend (active: %1; available: "
               "%2). nereusd cannot serve wss:// until a working Qt TLS "
               "backend is available.")
        .arg(activeDisplay, availableDisplay);
}

} // namespace NereusSDR
