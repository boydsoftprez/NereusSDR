// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/roundtable/RoundtableKey.cpp  (Longpath)
// =================================================================
//
// Siehe RoundtableKey.h.
//
// Die festen Byte-Folgen hier sind keine erfundenen Zahlen, sondern DER:
// die SubjectPublicKeyInfo eines P-256-Schluessels hat immer denselben
// Kopf (Protokoll v1, 4.1: "canonical 91-byte P-256 SubjectPublicKeyInfo"),
// und PKCS#8 nach RFC 5208 mit dem ECPrivateKey nach RFC 5915 ebenso —
// so wie OpenSSL (eckey_priv_encode, ohne die Kurve im ECPrivateKey) und
// Pythons cryptography ihn schreiben. Wer andere Dateien liest, muss sie
// nicht kennen; wir schreiben nur diese eine Form.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/roundtable/RoundtableKey.h"

#include "core/LogCategories.h"
#include "core/roundtable/RoundtableWire.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSaveFile>

#include <mbedtls/bignum.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md.h>

#include <array>
#include <cstring>

namespace Longpath {

namespace {

constexpr int kScalarBytes = 32;
constexpr int kPointBytes = 65; // 0x04 || X || Y
constexpr int kSpkiBytes = 91;
constexpr int kSignatureBytes = 64;
constexpr qint64 kMaxKeyFileBytes = 16384;

// SubjectPublicKeyInfo { AlgorithmIdentifier { id-ecPublicKey, prime256v1 },
// BIT STRING (65 Byte, keine ungenutzten Bits) } — die 26 Byte vor dem Punkt.
constexpr std::array<unsigned char, 26> kSpkiPrefix = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00};

// PKCS#8 PrivateKeyInfo { 0, AlgorithmIdentifier, OCTET STRING { ECPrivateKey
// { 1, OCTET STRING d, [1] BIT STRING Q } } } — 138 Byte: 36 vor d, 5
// zwischen d und Q.
constexpr std::array<unsigned char, 36> kPkcs8Prefix = {
    0x30, 0x81, 0x87, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x03, 0x01, 0x07, 0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20};
constexpr std::array<unsigned char, 5> kPkcs8Middle = {0xa1, 0x44, 0x03, 0x42, 0x00};
constexpr int kPkcs8Bytes = 36 + kScalarBytes + 5 + kPointBytes; // 138

// Dieselbe Form ohne den oeffentlichen Teil (RFC 5915 erlaubt ihn wegzulassen;
// manche Werkzeuge tun das). Nur gelesen, nie geschrieben.
constexpr std::array<unsigned char, 35> kPkcs8ShortPrefix = {
    0x30, 0x41, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48,
    0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03,
    0x01, 0x07, 0x04, 0x27, 0x30, 0x25, 0x02, 0x01, 0x01, 0x04, 0x20};
constexpr int kPkcs8ShortBytes = 35 + kScalarBytes; // 67

template <std::size_t N>
bool startsWith(const QByteArray& bytes, const std::array<unsigned char, N>& prefix)
{
    if (bytes.size() < qsizetype(N)) {
        return false;
    }
    return std::memcmp(bytes.constData(), prefix.data(), N) == 0;
}

// Der Zufall des Betriebssystems fuer mbedTLS: neuer Schluessel und die
// Blendung gegen Seitenkanaele. QRandomGenerator::system() ist laut Qt fuer
// kryptografische Zwecke gedacht (getrandom/arc4random/BCryptGenRandom).
int systemRandom(void* /*unused*/, unsigned char* out, size_t len)
{
    QRandomGenerator* gen = QRandomGenerator::system();
    size_t i = 0;
    while (i < len) {
        const quint32 word = gen->generate();
        const size_t n = qMin<size_t>(sizeof(word), len - i);
        std::memcpy(out + i, &word, n);
        i += n;
    }
    return 0;
}

QByteArray sha256(const QByteArray& message)
{
    return QCryptographicHash::hash(message, QCryptographicHash::Sha256);
}

void setError(QString* error, const QString& text)
{
    if (error != nullptr) {
        *error = text;
    }
}

// Den oeffentlichen Punkt zu einem geheimen Skalar. Prueft dabei, dass d
// in [1, n-1] liegt (mbedtls_ecp_read_key).
QByteArray publicPointFor(const QByteArray& d, QString* error)
{
    mbedtls_ecp_keypair kp;
    mbedtls_ecp_keypair_init(&kp);
    QByteArray point(kPointBytes, '\0');
    size_t olen = 0;
    int rc = mbedtls_ecp_read_key(MBEDTLS_ECP_DP_SECP256R1, &kp,
                                  reinterpret_cast<const unsigned char*>(d.constData()),
                                  size_t(d.size()));
    if (rc == 0) {
        rc = mbedtls_ecp_keypair_calc_public(&kp, systemRandom, nullptr);
    }
    if (rc == 0) {
        rc = mbedtls_ecp_write_public_key(&kp, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen,
                                          reinterpret_cast<unsigned char*>(point.data()),
                                          size_t(point.size()));
    }
    mbedtls_ecp_keypair_free(&kp);
    if (rc != 0 || olen != size_t(kPointBytes)) {
        setError(error, QStringLiteral("Kein gueltiger P-256-Schluessel (mbedTLS %1)").arg(rc));
        return {};
    }
    return point;
}

} // namespace

struct RoundtableKey::Impl {
    QByteArray d;     // 32 Byte, big-endian
    QByteArray point; // 65 Byte, 0x04 || X || Y

    ~Impl() { d.fill('\0'); }
};

RoundtableKey::RoundtableKey() = default;
RoundtableKey::~RoundtableKey() = default;
RoundtableKey::RoundtableKey(RoundtableKey&&) noexcept = default;
RoundtableKey& RoundtableKey::operator=(RoundtableKey&&) noexcept = default;

bool RoundtableKey::isValid() const
{
    return m_impl && m_impl->d.size() == kScalarBytes && m_impl->point.size() == kPointBytes;
}

RoundtableKey RoundtableKey::fromPrivateScalar(const QByteArray& d, QString* error)
{
    RoundtableKey key;
    if (d.size() != kScalarBytes) {
        setError(error, QStringLiteral("Der geheime Skalar muss 32 Byte haben, nicht %1")
                            .arg(d.size()));
        return key;
    }
    const QByteArray point = publicPointFor(d, error);
    if (point.isEmpty()) {
        return key;
    }
    key.m_impl = std::make_unique<Impl>();
    key.m_impl->d = d;
    key.m_impl->point = point;
    return key;
}

RoundtableKey RoundtableKey::generate(QString* error)
{
    mbedtls_ecp_keypair kp;
    mbedtls_ecp_keypair_init(&kp);
    QByteArray d(kScalarBytes, '\0');
    size_t olen = 0;
    int rc = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, &kp, systemRandom, nullptr);
    if (rc == 0) {
        rc = mbedtls_ecp_write_key_ext(&kp, &olen, reinterpret_cast<unsigned char*>(d.data()),
                                       size_t(d.size()));
    }
    mbedtls_ecp_keypair_free(&kp);
    if (rc != 0 || olen != size_t(kScalarBytes)) {
        d.fill('\0');
        setError(error, QStringLiteral("Schluessel konnte nicht erzeugt werden (mbedTLS %1)")
                            .arg(rc));
        return {};
    }
    RoundtableKey key = fromPrivateScalar(d, error);
    d.fill('\0');
    return key;
}

RoundtableKey RoundtableKey::fromPkcs8Pem(const QByteArray& pem, QString* error)
{
    static const QByteArray kBegin("-----BEGIN PRIVATE KEY-----");
    static const QByteArray kEnd("-----END PRIVATE KEY-----");
    const qsizetype b = pem.indexOf(kBegin);
    const qsizetype e = pem.indexOf(kEnd);
    if (b < 0 || e < 0 || e < b) {
        setError(error, QStringLiteral("Keine PKCS#8-Datei (BEGIN PRIVATE KEY fehlt)"));
        return {};
    }
    QByteArray body = pem.mid(b + kBegin.size(), e - b - kBegin.size());
    body = body.simplified().replace(' ', QByteArray());
    QByteArray der = QByteArray::fromBase64(body);
    body.fill('\0');

    QByteArray d;
    QByteArray embeddedPoint;
    if (der.size() == kPkcs8Bytes && startsWith(der, kPkcs8Prefix)
        && std::memcmp(der.constData() + 36 + kScalarBytes, kPkcs8Middle.data(),
                       kPkcs8Middle.size())
               == 0
        && static_cast<unsigned char>(der.at(36 + kScalarBytes + 5)) == 0x04) {
        d = der.mid(36, kScalarBytes);
        embeddedPoint = der.mid(36 + kScalarBytes + 5, kPointBytes);
    } else if (der.size() == kPkcs8ShortBytes && startsWith(der, kPkcs8ShortPrefix)) {
        d = der.mid(35, kScalarBytes);
    } else {
        der.fill('\0');
        setError(error, QStringLiteral("Die Datei haelt keinen P-256-Schluessel in der "
                                       "erwarteten PKCS#8-Form"));
        return {};
    }
    der.fill('\0');

    RoundtableKey key = fromPrivateScalar(d, error);
    d.fill('\0');
    if (key.isValid() && !embeddedPoint.isEmpty() && embeddedPoint != key.m_impl->point) {
        setError(error, QStringLiteral("Oeffentlicher und geheimer Teil der Datei passen "
                                       "nicht zusammen"));
        return {};
    }
    return key;
}

RoundtableKey RoundtableKey::loadOrCreate(const QString& path, QString* error)
{
    QFile file(path);
    if (file.exists()) {
#ifndef Q_OS_WIN
        // Wie Upstreams StationIdentity: eine fuer andere lesbare Datei wird
        // wieder nur dem Benutzer gegeben, mit einer Warnung.
        const QFileDevice::Permissions others =
            QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
        if ((file.permissions() & others) != QFileDevice::Permissions()) {
            if (file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
                qCWarning(lcSpots) << "Roundtable:" << path
                                   << "war fuer andere lesbar; jetzt wieder 0600";
            } else {
                setError(error, QStringLiteral("%1 ist fuer andere lesbar und laesst sich "
                                               "nicht auf 0600 stellen: %2")
                                    .arg(path, file.errorString()));
                return {};
            }
        }
#endif
        if (!file.open(QIODevice::ReadOnly)) {
            setError(error, QStringLiteral("%1 laesst sich nicht lesen: %2")
                                .arg(path, file.errorString()));
            return {};
        }
        QByteArray pem = file.read(kMaxKeyFileBytes);
        RoundtableKey key = fromPkcs8Pem(pem, error);
        pem.fill('\0');
        return key;
    }

    RoundtableKey key = generate(error);
    if (!key.isValid()) {
        return {};
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QByteArray pem = key.pkcs8Pem();
    QSaveFile out(path);
    // 0600 auf die Zwischendatei, BEVOR sie unter dem endgueltigen Namen
    // steht — so liegt der Schluessel nie mit weiteren Rechten da.
    const bool ok = out.open(QIODevice::WriteOnly)
                    && out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
                    && out.write(pem) == pem.size() && out.commit();
    pem.fill('\0');
    if (!ok) {
        setError(error, QStringLiteral("%1 laesst sich nicht schreiben: %2")
                            .arg(path, out.errorString()));
        return {};
    }
    qCInfo(lcSpots) << "Roundtable: neuer Stationsschluessel angelegt," << path;
    return key;
}

QByteArray RoundtableKey::spkiDer() const
{
    if (!isValid()) {
        return {};
    }
    QByteArray spki(reinterpret_cast<const char*>(kSpkiPrefix.data()), qsizetype(kSpkiPrefix.size()));
    spki.append(m_impl->point);
    return spki;
}

QString RoundtableKey::spkiBase64Url() const
{
    return isValid() ? RoundtableWire::toBase64Url(spkiDer()) : QString();
}

QString RoundtableKey::roundtableId() const
{
    return isValid() ? RoundtableWire::roundtableId(spkiDer()) : QString();
}

QByteArray RoundtableKey::pkcs8Pem() const
{
    if (!isValid()) {
        return {};
    }
    QByteArray der(reinterpret_cast<const char*>(kPkcs8Prefix.data()), qsizetype(kPkcs8Prefix.size()));
    der.append(m_impl->d);
    der.append(reinterpret_cast<const char*>(kPkcs8Middle.data()), qsizetype(kPkcs8Middle.size()));
    der.append(m_impl->point);
    QByteArray b64 = der.toBase64();
    der.fill('\0');
    QByteArray pem("-----BEGIN PRIVATE KEY-----\n");
    for (qsizetype i = 0; i < b64.size(); i += 64) {
        pem.append(b64.mid(i, 64));
        pem.append('\n');
    }
    pem.append("-----END PRIVATE KEY-----\n");
    b64.fill('\0');
    return pem;
}

QByteArray RoundtableKey::sign(const QByteArray& message) const
{
    if (!isValid()) {
        return {};
    }
    const QByteArray hash = sha256(message);
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_mpi r;
    mbedtls_mpi s;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    QByteArray sig(kSignatureBytes, '\0');
    int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (rc == 0) {
        rc = mbedtls_mpi_read_binary(&d, reinterpret_cast<const unsigned char*>(m_impl->d.constData()),
                                     size_t(m_impl->d.size()));
    }
    if (rc == 0) {
        rc = mbedtls_ecdsa_sign_det_ext(&grp, &r, &s, &d,
                                        reinterpret_cast<const unsigned char*>(hash.constData()),
                                        size_t(hash.size()), MBEDTLS_MD_SHA256, systemRandom,
                                        nullptr);
    }
    if (rc == 0) {
        rc = mbedtls_mpi_write_binary(&r, reinterpret_cast<unsigned char*>(sig.data()), 32);
    }
    if (rc == 0) {
        rc = mbedtls_mpi_write_binary(&s, reinterpret_cast<unsigned char*>(sig.data()) + 32, 32);
    }
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    if (rc != 0) {
        qCWarning(lcSpots) << "Roundtable: Signatur fehlgeschlagen, mbedTLS" << rc;
        return {};
    }
    return sig;
}

bool RoundtableKey::isCanonicalSpki(const QByteArray& spkiDer)
{
    if (spkiDer.size() != kSpkiBytes || !startsWith(spkiDer, kSpkiPrefix)
        || static_cast<unsigned char>(spkiDer.at(26)) != 0x04) {
        return false;
    }
    mbedtls_ecp_group grp;
    mbedtls_ecp_point q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&q);
    int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (rc == 0) {
        rc = mbedtls_ecp_point_read_binary(
            &grp, &q, reinterpret_cast<const unsigned char*>(spkiDer.constData()) + 26,
            size_t(kPointBytes));
    }
    if (rc == 0) {
        rc = mbedtls_ecp_check_pubkey(&grp, &q);
    }
    mbedtls_ecp_point_free(&q);
    mbedtls_ecp_group_free(&grp);
    return rc == 0;
}

bool RoundtableKey::verify(const QByteArray& spkiDer, const QByteArray& message,
                           const QByteArray& signature)
{
    if (signature.size() != kSignatureBytes || !isCanonicalSpki(spkiDer)) {
        return false;
    }
    const QByteArray hash = sha256(message);
    mbedtls_ecp_group grp;
    mbedtls_ecp_point q;
    mbedtls_mpi r;
    mbedtls_mpi s;
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&q);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    const auto* sig = reinterpret_cast<const unsigned char*>(signature.constData());
    int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (rc == 0) {
        rc = mbedtls_ecp_point_read_binary(
            &grp, &q, reinterpret_cast<const unsigned char*>(spkiDer.constData()) + 26,
            size_t(kPointBytes));
    }
    if (rc == 0) {
        rc = mbedtls_mpi_read_binary(&r, sig, 32);
    }
    if (rc == 0) {
        rc = mbedtls_mpi_read_binary(&s, sig + 32, 32);
    }
    if (rc == 0) {
        // Prueft auch 1 <= r, s <= n-1 (Protokoll 4.1); ein hohes s ist gueltig.
        rc = mbedtls_ecdsa_verify(&grp, reinterpret_cast<const unsigned char*>(hash.constData()),
                                  size_t(hash.size()), &q, &r, &s);
    }
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&r);
    mbedtls_ecp_point_free(&q);
    mbedtls_ecp_group_free(&grp);
    return rc == 0;
}

} // namespace Longpath
