// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/StationIdentity.cpp  (NereusSDR)
// =================================================================
// See StationIdentity.h for the design and the wire encodings.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: iPhone app Task 28 fix wave (review Important 3): every
//               OpenSSL entry point leaves this thread's error queue empty
//               (OpenSslErrorScope). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/security/StationIdentity.h"

#include "core/LogCategories.h"
#include "core/security/OpenSslErrorScope.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QSaveFile>

namespace NereusSDR {

namespace {

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using EcdsaSigPtr = std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)>;

// A key file is a few hundred bytes; the bound keeps a corrupt or hostile
// file from making the Core allocate before it validates anything.
constexpr qint64 kMaxKeyFileBytes = 16 * 1024;
constexpr int kCoordinateBytes = 32;

const QByteArray kCertBindingPrefix = QByteArrayLiteral("NereusSDR cert-binding v1\n");

// True for an EC key on P-256 (OpenSSL's name for it is prime256v1).
bool isP256Key(EVP_PKEY* key)
{
    if (key == nullptr || EVP_PKEY_get_base_id(key) != EVP_PKEY_EC) {
        return false;
    }
    char group[64] = {};
    size_t length = 0;
    if (EVP_PKEY_get_utf8_string_param(key, OSSL_PKEY_PARAM_GROUP_NAME, group, sizeof(group),
                                       &length)
        != 1) {
        return false;
    }
    return QByteArray(group, static_cast<int>(length)) == QByteArrayLiteral("prime256v1");
}

QByteArray spkiOf(EVP_PKEY* key)
{
    unsigned char* der = nullptr;
    const int length = i2d_PUBKEY(key, &der);
    if (length <= 0 || der == nullptr) {
        return {};
    }
    QByteArray out(reinterpret_cast<const char*>(der), length);
    OPENSSL_free(der);
    return out;
}

EvpPkeyPtr publicKeyFromSpki(const QByteArray& spki)
{
    const auto* cursor = reinterpret_cast<const unsigned char*>(spki.constData());
    EVP_PKEY* key = d2i_PUBKEY(nullptr, &cursor, spki.size());
    EvpPkeyPtr owned(key, &EVP_PKEY_free);
    // The whole input, and nothing after it.
    if (key == nullptr
        || cursor != reinterpret_cast<const unsigned char*>(spki.constData()) + spki.size()) {
        return EvpPkeyPtr(nullptr, &EVP_PKEY_free);
    }
    return owned;
}

// Raw r || s to the DER form OpenSSL verifies.
QByteArray derFromRaw(const QByteArray& raw)
{
    if (raw.size() != StationIdentity::kSignatureBytes) {
        return {};
    }
    EcdsaSigPtr sig(ECDSA_SIG_new(), &ECDSA_SIG_free);
    if (!sig) {
        return {};
    }
    BIGNUM* r = BN_bin2bn(reinterpret_cast<const unsigned char*>(raw.constData()),
                          kCoordinateBytes, nullptr);
    BIGNUM* s = BN_bin2bn(reinterpret_cast<const unsigned char*>(raw.constData())
                              + kCoordinateBytes,
                          kCoordinateBytes, nullptr);
    if (r == nullptr || s == nullptr || ECDSA_SIG_set0(sig.get(), r, s) != 1) {
        BN_free(r);
        BN_free(s);
        return {};
    }
    unsigned char* der = nullptr;
    const int length = i2d_ECDSA_SIG(sig.get(), &der);
    if (length <= 0 || der == nullptr) {
        return {};
    }
    QByteArray out(reinterpret_cast<const char*>(der), length);
    OPENSSL_free(der);
    return out;
}

// DER from OpenSSL to raw r || s.
QByteArray rawFromDer(const QByteArray& der)
{
    const auto* cursor = reinterpret_cast<const unsigned char*>(der.constData());
    EcdsaSigPtr sig(d2i_ECDSA_SIG(nullptr, &cursor, der.size()), &ECDSA_SIG_free);
    if (!sig) {
        return {};
    }
    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(sig.get(), &r, &s);
    QByteArray out(StationIdentity::kSignatureBytes, '\0');
    auto* bytes = reinterpret_cast<unsigned char*>(out.data());
    if (BN_bn2binpad(r, bytes, kCoordinateBytes) != kCoordinateBytes
        || BN_bn2binpad(s, bytes + kCoordinateBytes, kCoordinateBytes) != kCoordinateBytes) {
        return {};
    }
    return out;
}

} // namespace

StationIdentity::StationIdentity() = default;

StationIdentity StationIdentity::loadOrCreate(const QString& profileDir)
{
    return loadOrCreateKeyFile(profileDir, QString::fromLatin1(kKeyFileName),
                               QStringLiteral("The Core's"));
}

bool StationIdentity::keepOwnerOnly(const QString& path)
{
#ifdef Q_OS_WIN
    Q_UNUSED(path);
    return true;
#else
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    const QFileDevice::Permissions others =
        QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    if ((file.permissions() & others) == QFileDevice::Permissions()) {
        return true;
    }
    if (file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        qCWarning(lcConnection) << path
                                << "could be read by other users; it is owner-only again "
                                   "(mode 0600)";
        return true;
    }
    qCWarning(lcConnection) << path
                            << "can be read by other users and could not be made owner-only "
                               "(mode 0600):"
                            << file.errorString();
    return false;
#endif
}

StationIdentity StationIdentity::loadOrCreateKeyFile(const QString& profileDir,
                                                     const QString& fileName,
                                                     const QString& whose)
{
    const OpenSslErrorScope openSslErrors;
    StationIdentity identity;
    identity.m_keyPath = QDir(profileDir).filePath(fileName);
    // Only this function's own log text uses it ("The Core's", "This
    // computer's"); the sentence below begins with it.
    const QString lowerWhose = whose.isEmpty() ? whose : whose.at(0).toLower() + whose.mid(1);

    if (!QDir().mkpath(profileDir)) {
        identity.m_lastError = QStringLiteral("Could not create %1").arg(profileDir);
        return identity;
    }

    QFile file(identity.m_keyPath);
    if (file.exists()) {
        keepOwnerOnly(identity.m_keyPath);
        if (!file.open(QIODevice::ReadOnly)) {
            identity.m_lastError = QStringLiteral("%1 identity key %2 could not be read: %3")
                                       .arg(whose, identity.m_keyPath, file.errorString());
            return identity;
        }
        const QByteArray pem = file.read(kMaxKeyFileBytes);
        file.close();
        BioPtr bio(BIO_new_mem_buf(pem.constData(), static_cast<int>(pem.size())), &BIO_free);
        EVP_PKEY* raw = bio ? PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr)
                            : nullptr;
        EvpPkeyPtr key(raw, &EVP_PKEY_free);
        if (!key || !isP256Key(key.get())) {
            // Never regenerated over: a new key is a new Core, and every
            // paired device would have to pair again.
            identity.m_lastError =
                QStringLiteral("%1 identity key %2 is not a P-256 private key")
                    .arg(whose, identity.m_keyPath);
            return identity;
        }
        identity.m_spki = spkiOf(key.get());
        identity.m_key.reset(key.release(), &EVP_PKEY_free);
        return identity;
    }

    EvpPkeyPtr key(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256"), &EVP_PKEY_free);
    if (!key) {
        identity.m_lastError = QStringLiteral("Could not create %1 identity key").arg(lowerWhose);
        return identity;
    }
    BioPtr bio(BIO_new(BIO_s_mem()), &BIO_free);
    // PEM_write_bio_PrivateKey writes PKCS#8 ("BEGIN PRIVATE KEY").
    if (!bio || PEM_write_bio_PrivateKey(bio.get(), key.get(), nullptr, nullptr, 0, nullptr,
                                         nullptr)
                    != 1) {
        identity.m_lastError = QStringLiteral("Could not encode %1 identity key").arg(lowerWhose);
        return identity;
    }
    char* data = nullptr;
    const long length = BIO_get_mem_data(bio.get(), &data);
    QByteArray pem(data, static_cast<int>(length));

    // Mode 0600 on QSaveFile's own temporary file BEFORE commit(), so no
    // file ever exists under the final name with wider permissions (the
    // ordering CertificateStore::writePemFile() and TokenStore use).
    QSaveFile out(identity.m_keyPath);
    const bool written = out.open(QIODevice::WriteOnly)
                         && out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
                         && out.write(pem) == pem.size() && out.commit();
    // Scrub the in-memory PEM copy before it goes.
    pem.fill('\0');
    if (!written) {
        out.cancelWriting();
        identity.m_lastError = QStringLiteral("Could not write %1 identity key %2")
                                   .arg(lowerWhose, identity.m_keyPath);
        return identity;
    }
    identity.m_spki = spkiOf(key.get());
    identity.m_key.reset(key.release(), &EVP_PKEY_free);
    identity.m_createdThisRun = true;
    return identity;
}

QByteArray StationIdentity::fingerprint() const
{
    return isValid() ? fingerprintOf(m_spki) : QByteArray();
}

QByteArray StationIdentity::sign(const QByteArray& message) const
{
    const OpenSslErrorScope openSslErrors;
    if (!isValid()) {
        return {};
    }
    EvpMdCtxPtr ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!ctx
        || EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, m_key.get()) != 1) {
        return {};
    }
    size_t length = 0;
    const auto* input = reinterpret_cast<const unsigned char*>(message.constData());
    if (EVP_DigestSign(ctx.get(), nullptr, &length, input, static_cast<size_t>(message.size()))
        != 1) {
        return {};
    }
    QByteArray der(static_cast<int>(length), '\0');
    if (EVP_DigestSign(ctx.get(), reinterpret_cast<unsigned char*>(der.data()), &length, input,
                       static_cast<size_t>(message.size()))
        != 1) {
        return {};
    }
    der.truncate(static_cast<int>(length));
    return rawFromDer(der);
}

bool StationIdentity::verify(const QByteArray& spki, const QByteArray& message,
                             const QByteArray& signature)
{
    // A device's key or signature can be anything before sign-in: an
    // off-curve point or r = 0 makes OpenSSL queue an error.
    const OpenSslErrorScope openSslErrors;
    if (!isP256Spki(spki) || signature.size() != kSignatureBytes) {
        return false;
    }
    EvpPkeyPtr key = publicKeyFromSpki(spki);
    const QByteArray der = derFromRaw(signature);
    if (!key || der.isEmpty()) {
        return false;
    }
    EvpMdCtxPtr ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!ctx || EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1) {
        return false;
    }
    return EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char*>(der.constData()),
                            static_cast<size_t>(der.size()),
                            reinterpret_cast<const unsigned char*>(message.constData()),
                            static_cast<size_t>(message.size()))
           == 1;
}

QByteArray StationIdentity::certBindingMessage(const QByteArray& certSha256)
{
    return kCertBindingPrefix + certSha256;
}

QByteArray StationIdentity::certBinding(const QByteArray& certSha256) const
{
    return sign(certBindingMessage(certSha256));
}

QByteArray StationIdentity::fingerprintOf(const QByteArray& spki)
{
    return QCryptographicHash::hash(spki, QCryptographicHash::Sha256);
}

bool StationIdentity::isP256Spki(const QByteArray& spki)
{
    const OpenSslErrorScope openSslErrors;
    if (spki.size() != kSpkiBytes) {
        return false;
    }
    EvpPkeyPtr key = publicKeyFromSpki(spki);
    // Canonical: re-encoding gives the same bytes, so the fingerprint of a
    // key is one value whoever computes it.
    return key && isP256Key(key.get()) && spkiOf(key.get()) == spki;
}

QString StationIdentity::toBase64Url(const QByteArray& bytes)
{
    return QString::fromLatin1(
        bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray StationIdentity::fromBase64Url(const QString& text, bool* ok)
{
    const auto fail = [ok]() {
        if (ok != nullptr) {
            *ok = false;
        }
        return QByteArray();
    };
    for (const QChar c : text) {
        const ushort u = c.unicode();
        const bool alphabet = (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z')
                              || (u >= '0' && u <= '9') || u == '-' || u == '_';
        if (!alphabet) {
            return fail();
        }
    }
    // A length of 1 mod 4 is never the unpadded form of any bytes.
    if (text.size() % 4 == 1) {
        return fail();
    }
    const auto decoded = QByteArray::fromBase64Encoding(
        text.toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals
            | QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) {
        return fail();
    }
    // Canonical: the unused low bits of the last character are zero, so one
    // value has one text.
    if (toBase64Url(*decoded) != text) {
        return fail();
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return *decoded;
}

} // namespace NereusSDR
