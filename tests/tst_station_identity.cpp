// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_identity.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 12 (R-IOS-08): the Core's identity key.
//
// Refusals first: a damaged key file is never replaced, a key on another
// curve is refused, and verify() refuses every signature and key that is
// not exactly the link's form. Then the admit path: the key is created
// once (mode 0600, PKCS#8), reused on every later start with the same
// fingerprint, and its raw r || s signatures are the ones OpenSSL itself
// verifies (and the other way round), which is what another
// implementation (the phone's CryptoKit) has to meet.
//
// Keys are generated at run time in scratch directories; nothing here is
// a stored secret.
//
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
// =================================================================

#include <QtTest>

#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>

#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <memory>

#include "core/security/StationIdentity.h"

using namespace NereusSDR;

namespace {

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

QByteArray pemOf(EVP_PKEY* key)
{
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), &BIO_free);
    PEM_write_bio_PrivateKey(bio.get(), key, nullptr, nullptr, 0, nullptr, nullptr);
    char* data = nullptr;
    const long length = BIO_get_mem_data(bio.get(), &data);
    return QByteArray(data, static_cast<int>(length));
}

QByteArray spkiOf(EVP_PKEY* key)
{
    unsigned char* der = nullptr;
    const int length = i2d_PUBKEY(key, &der);
    QByteArray out(reinterpret_cast<const char*>(der), length);
    OPENSSL_free(der);
    return out;
}

// OpenSSL's own DER signature, for cross-checking the raw form.
QByteArray opensslDerSign(EVP_PKEY* key, const QByteArray& message)
{
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                               &EVP_MD_CTX_free);
    EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key);
    size_t length = 0;
    EVP_DigestSign(ctx.get(), nullptr, &length,
                   reinterpret_cast<const unsigned char*>(message.constData()),
                   static_cast<size_t>(message.size()));
    QByteArray der(static_cast<int>(length), '\0');
    EVP_DigestSign(ctx.get(), reinterpret_cast<unsigned char*>(der.data()), &length,
                   reinterpret_cast<const unsigned char*>(message.constData()),
                   static_cast<size_t>(message.size()));
    der.truncate(static_cast<int>(length));
    return der;
}

QByteArray rawOf(const QByteArray& der)
{
    const auto* cursor = reinterpret_cast<const unsigned char*>(der.constData());
    ECDSA_SIG* sig = d2i_ECDSA_SIG(nullptr, &cursor, der.size());
    const BIGNUM* r = nullptr;
    const BIGNUM* s = nullptr;
    ECDSA_SIG_get0(sig, &r, &s);
    QByteArray raw(64, '\0');
    BN_bn2binpad(r, reinterpret_cast<unsigned char*>(raw.data()), 32);
    BN_bn2binpad(s, reinterpret_cast<unsigned char*>(raw.data()) + 32, 32);
    ECDSA_SIG_free(sig);
    return raw;
}

QByteArray derOf(const QByteArray& raw)
{
    ECDSA_SIG* sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(sig,
                   BN_bin2bn(reinterpret_cast<const unsigned char*>(raw.constData()), 32, nullptr),
                   BN_bin2bn(reinterpret_cast<const unsigned char*>(raw.constData()) + 32, 32,
                             nullptr));
    unsigned char* der = nullptr;
    const int length = i2d_ECDSA_SIG(sig, &der);
    QByteArray out(reinterpret_cast<const char*>(der), length);
    OPENSSL_free(der);
    ECDSA_SIG_free(sig);
    return out;
}

bool opensslVerifyDer(EVP_PKEY* key, const QByteArray& message, const QByteArray& der)
{
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                               &EVP_MD_CTX_free);
    EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key);
    return EVP_DigestVerify(ctx.get(), reinterpret_cast<const unsigned char*>(der.constData()),
                            static_cast<size_t>(der.size()),
                            reinterpret_cast<const unsigned char*>(message.constData()),
                            static_cast<size_t>(message.size()))
           == 1;
}

EvpPkeyPtr loadPem(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return EvpPkeyPtr(nullptr, &EVP_PKEY_free);
    }
    const QByteArray pem = file.readAll();
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(
        BIO_new_mem_buf(pem.constData(), static_cast<int>(pem.size())), &BIO_free);
    return EvpPkeyPtr(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr),
                      &EVP_PKEY_free);
}

QStringList g_logged;
void captureMessages(QtMsgType, const QMessageLogContext&, const QString& message)
{
    g_logged.append(message);
}

} // namespace

class TstStationIdentity : public QObject {
    Q_OBJECT

private slots:
    // ── Refusals ─────────────────────────────────────────────────────────

    void aDamagedKeyFileIsRefusedAndNeverReplaced()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QString::fromLatin1(StationIdentity::kKeyFileName));
        QFile damaged(path);
        QVERIFY(damaged.open(QIODevice::WriteOnly));
        damaged.write("-----BEGIN PRIVATE KEY-----\nnot a key\n-----END PRIVATE KEY-----\n");
        damaged.close();
        const QByteArray before = [&path]() {
            QFile f(path);
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        }();

        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        QVERIFY(!identity.isValid());
        QVERIFY(!identity.lastError().isEmpty());
        QVERIFY(identity.publicKeySpki().isEmpty());
        QVERIFY(identity.sign(QByteArrayLiteral("x")).isEmpty());

        QFile after(path);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), before);
    }

    void aKeyOnAnotherCurveIsRefused()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        EvpPkeyPtr p384(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-384"), &EVP_PKEY_free);
        QVERIFY(p384);
        QFile file(dir.filePath(QString::fromLatin1(StationIdentity::kKeyFileName)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(pemOf(p384.get()));
        file.close();

        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        QVERIFY(!identity.isValid());
    }

    void verifyRefusesEverythingButTheLinksForm()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        QVERIFY2(identity.isValid(), qPrintable(identity.lastError()));
        const QByteArray message = QByteArrayLiteral("NereusSDR device-auth v1\nmessage");
        const QByteArray signature = identity.sign(message);
        QCOMPARE(signature.size(), StationIdentity::kSignatureBytes);
        QVERIFY(StationIdentity::verify(identity.publicKeySpki(), message, signature));

        // Another message.
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(), message + 'x', signature));
        // A flipped bit in r, and in s.
        QByteArray flipped = signature;
        flipped[3] = static_cast<char>(flipped[3] ^ 0x01);
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(), message, flipped));
        flipped = signature;
        flipped[40] = static_cast<char>(flipped[40] ^ 0x80);
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(), message, flipped));
        // Wrong lengths, including OpenSSL's own DER form.
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(), message, signature.left(63)));
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(), message, signature + '\0'));
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(), message, derOf(signature)));
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(), message, QByteArray(64, '\0')));

        // Another key.
        QTemporaryDir otherDir;
        const StationIdentity other = StationIdentity::loadOrCreate(otherDir.path());
        QVERIFY(!StationIdentity::verify(other.publicKeySpki(), message, signature));

        // Keys that are not a canonical P-256 SubjectPublicKeyInfo.
        const QByteArray spki = identity.publicKeySpki();
        QCOMPARE(spki.size(), StationIdentity::kSpkiBytes);
        QVERIFY(StationIdentity::isP256Spki(spki));
        QVERIFY(!StationIdentity::verify(spki.left(90), message, signature));
        QVERIFY(!StationIdentity::verify(spki + '\0', message, signature));
        QVERIFY(!StationIdentity::verify(QByteArray(), message, signature));
        EvpPkeyPtr p384(EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-384"), &EVP_PKEY_free);
        QVERIFY(!StationIdentity::isP256Spki(spkiOf(p384.get())));
        EvpPkeyPtr ed(EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519"), &EVP_PKEY_free);
        QVERIFY(!StationIdentity::isP256Spki(spkiOf(ed.get())));
    }

    void base64UrlIsStrict()
    {
        bool ok = true;
        const QByteArray bytes = QByteArray::fromHex("fbff00");
        QCOMPARE(StationIdentity::toBase64Url(bytes), QStringLiteral("-_8A"));
        QCOMPARE(StationIdentity::fromBase64Url(QStringLiteral("-_8A"), &ok), bytes);
        QVERIFY(ok);
        // Padding, the standard alphabet, spaces and an impossible length.
        for (const QString& text : {QStringLiteral("-_8A="), QStringLiteral("+/8A"),
                                    QStringLiteral("-_8 A"), QStringLiteral("AAAAA")}) {
            StationIdentity::fromBase64Url(text, &ok);
            QVERIFY2(!ok, qPrintable(text));
        }
        // Non-zero unused bits: "AB" and "AA" would both decode to 0x00.
        StationIdentity::fromBase64Url(QStringLiteral("AB"), &ok);
        QVERIFY(!ok);
        StationIdentity::fromBase64Url(QStringLiteral("AA"), &ok);
        QVERIFY(ok);
    }

    // ── The admit path ───────────────────────────────────────────────────

    void aKeyRestoredAtWiderPermissionsIsMadeOwnerOnlyOnLoad()
    {
        // Part C fix wave (R1-M5): a key file restored from a backup at
        // 0644 is set back to 0600 when it is loaded, and still loads.
#ifdef Q_OS_WIN
        QSKIP("Windows files carry no Unix mode; the profile directory's ACL protects them.");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const StationIdentity first = StationIdentity::loadOrCreate(dir.path());
        QVERIFY(first.isValid());
        QFile file(first.keyPath());
        QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                    | QFileDevice::ReadGroup | QFileDevice::ReadOther));
        const StationIdentity again = StationIdentity::loadOrCreate(dir.path());
        QVERIFY2(again.isValid(), qPrintable(again.lastError()));
        QCOMPARE(again.fingerprint(), first.fingerprint());
        const QFileDevice::Permissions others =
            QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
        QCOMPARE(QFile(first.keyPath()).permissions() & others, QFileDevice::Permissions());
        QVERIFY(QFile(first.keyPath()).permissions().testFlag(QFileDevice::ReadOwner));
#endif
    }

    void theFirstStartCreatesAnOwnerOnlyPkcs8Key()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        QVERIFY2(identity.isValid(), qPrintable(identity.lastError()));
        QVERIFY(identity.wasCreatedThisRun());
        QCOMPARE(identity.keyPath(),
                 dir.filePath(QString::fromLatin1(StationIdentity::kKeyFileName)));

        QFile file(identity.keyPath());
        QVERIFY(file.exists());
#ifndef Q_OS_WIN
        QCOMPARE(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                       | QFileDevice::ReadOther | QFileDevice::WriteOther
                                       | QFileDevice::ExeGroup | QFileDevice::ExeOther),
                 QFileDevice::Permissions());
        QVERIFY(file.permissions().testFlag(QFileDevice::ReadOwner));
#endif
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray pem = file.readAll();
        QVERIFY(pem.startsWith("-----BEGIN PRIVATE KEY-----"));

        QCOMPARE(identity.publicKeySpki().size(), StationIdentity::kSpkiBytes);
        QCOMPARE(identity.fingerprint().size(), 32);
        QCOMPARE(identity.fingerprint(),
                 QCryptographicHash::hash(identity.publicKeySpki(), QCryptographicHash::Sha256));
    }

    void laterStartsReuseTheKeyAndItsFingerprint()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const StationIdentity first = StationIdentity::loadOrCreate(dir.path());
        QVERIFY(first.isValid());
        const StationIdentity second = StationIdentity::loadOrCreate(dir.path());
        QVERIFY2(second.isValid(), qPrintable(second.lastError()));
        QVERIFY(!second.wasCreatedThisRun());
        QCOMPARE(second.fingerprint(), first.fingerprint());
        QCOMPARE(second.publicKeySpki(), first.publicKeySpki());
        // The reloaded key signs for the same public key.
        const QByteArray message = QByteArrayLiteral("reloaded");
        QVERIFY(StationIdentity::verify(first.publicKeySpki(), message, second.sign(message)));

        QTemporaryDir otherDir;
        const StationIdentity other = StationIdentity::loadOrCreate(otherDir.path());
        QVERIFY(other.fingerprint() != first.fingerprint());
    }

    void signaturesAreRawRsThatOpensslAgreesWith()
    {
        QTemporaryDir dir;
        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        QVERIFY(identity.isValid());
        EvpPkeyPtr key = loadPem(identity.keyPath());
        QVERIFY(key);
        QCOMPARE(spkiOf(key.get()), identity.publicKeySpki());

        const QByteArray message = QByteArrayLiteral("cross-check");
        // Ours, read by OpenSSL as DER.
        QVERIFY(opensslVerifyDer(key.get(), message, derOf(identity.sign(message))));
        // OpenSSL's, read by ours as raw r || s.
        QVERIFY(StationIdentity::verify(identity.publicKeySpki(), message,
                                        rawOf(opensslDerSign(key.get(), message))));
    }

    void theCertificateBindingSignsItsPrefixAndTheHash()
    {
        QTemporaryDir dir;
        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        const QByteArray certSha256 =
            QCryptographicHash::hash(QByteArrayLiteral("certificate"), QCryptographicHash::Sha256);
        QCOMPARE(StationIdentity::certBindingMessage(certSha256),
                 QByteArrayLiteral("NereusSDR cert-binding v1\n") + certSha256);
        const QByteArray binding = identity.certBinding(certSha256);
        QVERIFY(StationIdentity::verify(identity.publicKeySpki(),
                                        StationIdentity::certBindingMessage(certSha256), binding));
        const QByteArray otherSha256 =
            QCryptographicHash::hash(QByteArrayLiteral("another"), QCryptographicHash::Sha256);
        QVERIFY(!StationIdentity::verify(identity.publicKeySpki(),
                                         StationIdentity::certBindingMessage(otherSha256),
                                         binding));
    }

    void theKeyNeverReachesTheLog()
    {
        QTemporaryDir dir;
        g_logged.clear();
        const QtMessageHandler previous = qInstallMessageHandler(captureMessages);
        const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
        const StationIdentity again = StationIdentity::loadOrCreate(dir.path());
        qInstallMessageHandler(previous);
        QVERIFY(identity.isValid() && again.isValid());

        QFile file(identity.keyPath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QList<QByteArray> lines = file.readAll().split('\n');
        for (const QString& logged : std::as_const(g_logged)) {
            QVERIFY2(!logged.contains(QStringLiteral("PRIVATE KEY")), qPrintable(logged));
            for (const QByteArray& line : lines) {
                if (line.size() > 16 && !line.startsWith("-----")) {
                    QVERIFY2(!logged.contains(QString::fromLatin1(line)), "key material logged");
                }
            }
        }
    }
};

QTEST_GUILESS_MAIN(TstStationIdentity)
#include "tst_station_identity.moc"
