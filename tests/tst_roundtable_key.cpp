// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_roundtable_key.cpp  (Longpath)
// =================================================================
//
// Der Stationsschluessel fuer Roundtable gegen Upstreams Pruefdaten
// (roundtable/conformance/v1/crypto, byte fuer byte uebernommen): jede
// Aussage hier hat der Upstream-Server mit OpenSSL ebenso getroffen. Was
// wir mit mbedTLS rechnen, muss dasselbe herausbringen.
//
// Modification history (Longpath):
//   2026-11-14 - Angelegt fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/roundtable/RoundtableKey.h"
#include "core/roundtable/RoundtableWire.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace Longpath;

namespace {

QJsonObject vectors(const QString& name)
{
    const QString root = QString::fromLocal8Bit(qgetenv("LONGPATH_SOURCE_DIR"));
    QFile f(root + QStringLiteral("/roundtable/conformance/v1/crypto/") + name);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

QByteArray hex(const QJsonValue& v)
{
    return QByteArray::fromHex(v.toString().toLatin1());
}

// Was Pythons cryptography (also OpenSSL) fuer den Pruefschluessel a als
// PKCS#8 schreibt. Unsere Datei muss genau so aussehen, damit Upstreams
// Werkzeuge und unsere Telefon-App sie lesen koennen.
const char* kVectorKeyPem =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgGjWDhu9RwLUsYyek\n"
    "0gk8/ZPWzvN011vXAjQEGiUOUqqhRANCAARjGVljkq5CsKBXBDges03ZDxrjA6NT\n"
    "Y36eRWAE1QFGWq6REh/jimL7j2AKTvo9LWbylJLljSpC6wiMx1OpOr7f\n"
    "-----END PRIVATE KEY-----\n";

} // namespace

class TstRoundtableKey : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qEnvironmentVariableIsEmpty("LONGPATH_SOURCE_DIR")) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt (ueber ctest starten)");
        }
        QVERIFY2(!vectors(QStringLiteral("roundtable-id.json")).isEmpty(),
                 "roundtable/conformance/v1/crypto fehlt");
    }

    void oeffentlicherSchluesselUndIdAusDemSkalar()
    {
        const QJsonArray cases = vectors(QStringLiteral("roundtable-id.json"))
                                     .value(QStringLiteral("cases")).toArray();
        QVERIFY(cases.size() >= 2);
        for (const QJsonValue& v : cases) {
            const QJsonObject c = v.toObject();
            QString err;
            const RoundtableKey key = RoundtableKey::fromPrivateScalar(
                hex(c.value(QStringLiteral("privateKeyHex"))), &err);
            QVERIFY2(key.isValid(), qPrintable(err));
            QCOMPARE(key.spkiDer().toHex(), c.value(QStringLiteral("spkiHex")).toString().toLatin1());
            QCOMPARE(key.spkiBase64Url(), c.value(QStringLiteral("publicKey")).toString());
            QCOMPARE(key.roundtableId(), c.value(QStringLiteral("id")).toString());
        }
    }

    void nurKanonischeSpki()
    {
        const QJsonArray cases = vectors(QStringLiteral("p256-spki.json"))
                                     .value(QStringLiteral("cases")).toArray();
        QVERIFY(cases.size() >= 5);
        for (const QJsonValue& v : cases) {
            const QJsonObject c = v.toObject();
            const bool want = c.value(QStringLiteral("valid")).toBool();
            QVERIFY2(RoundtableKey::isCanonicalSpki(hex(c.value(QStringLiteral("spkiHex")))) == want,
                     qPrintable(c.value(QStringLiteral("name")).toString()));
        }
    }

    void signaturenWieUpstream()
    {
        const QJsonObject h = vectors(QStringLiteral("hello-signature.json"));
        bool ok = false;
        const QByteArray nonce = RoundtableWire::fromBase64Url(
            h.value(QStringLiteral("nonce")).toString(), &ok);
        QVERIFY(ok);
        const QByteArray transcript = RoundtableWire::helloTranscript(
            h.value(QStringLiteral("host")).toString(), nonce);
        QCOMPARE(transcript.toHex(), h.value(QStringLiteral("transcriptHex")).toString().toLatin1());

        const QJsonArray cases = h.value(QStringLiteral("cases")).toArray();
        QVERIFY(cases.size() >= 10);
        int gueltig = 0;
        for (const QJsonValue& v : cases) {
            const QJsonObject c = v.toObject();
            const QString name = c.value(QStringLiteral("name")).toString();
            bool okKey = false;
            bool okSig = false;
            const QByteArray spki = RoundtableWire::fromBase64Url(
                c.value(QStringLiteral("publicKey")).toString(), &okKey);
            const QByteArray sig = RoundtableWire::fromBase64Url(
                c.value(QStringLiteral("signature")).toString(), &okSig);
            const bool got = okKey && okSig && RoundtableKey::verify(spki, transcript, sig);
            QVERIFY2(got == c.value(QStringLiteral("valid")).toBool(), qPrintable(name));
            gueltig += got ? 1 : 0;
        }
        // "signed" und "highS" — das hohe s muss mbedTLS annehmen.
        QCOMPARE(gueltig, 2);
    }

    void eigeneSignaturHaeltDerPruefungStand()
    {
        const QJsonObject h = vectors(QStringLiteral("hello-signature.json"));
        QString err;
        const RoundtableKey key = RoundtableKey::fromPrivateScalar(
            hex(h.value(QStringLiteral("key")).toObject().value(QStringLiteral("privateKeyHex"))), &err);
        QVERIFY2(key.isValid(), qPrintable(err));
        bool ok = false;
        const QByteArray nonce = RoundtableWire::fromBase64Url(
            h.value(QStringLiteral("nonce")).toString(), &ok);
        const QByteArray transcript = RoundtableWire::helloTranscript(
            h.value(QStringLiteral("host")).toString(), nonce);

        const QByteArray sig = key.sign(transcript);
        QCOMPARE(sig.size(), 64);
        QVERIFY(RoundtableKey::verify(key.spkiDer(), transcript, sig));
        // RFC 6979: dieselbe Nachricht, dieselbe Signatur.
        QCOMPARE(key.sign(transcript), sig);
        // Ueber ein anderes Transkript gilt sie nicht.
        QVERIFY(!RoundtableKey::verify(key.spkiDer(),
                                       RoundtableWire::helloTranscript(
                                           QStringLiteral("roundtable.example.invalid"), nonce),
                                       sig));
        // Und die Gegenstelle — Upstreams Server — sieht nur base64url.
        QCOMPARE(RoundtableWire::toBase64Url(sig).size(), 86);
    }

    void pemWieOpenSsl()
    {
        const QJsonObject h = vectors(QStringLiteral("hello-signature.json"));
        const RoundtableKey key = RoundtableKey::fromPrivateScalar(
            hex(h.value(QStringLiteral("key")).toObject().value(QStringLiteral("privateKeyHex"))));
        QVERIFY(key.isValid());
        QCOMPARE(QString::fromLatin1(key.pkcs8Pem()), QString::fromLatin1(kVectorKeyPem));

        QString err;
        const RoundtableKey back = RoundtableKey::fromPkcs8Pem(QByteArray(kVectorKeyPem), &err);
        QVERIFY2(back.isValid(), qPrintable(err));
        QCOMPARE(back.spkiDer(), key.spkiDer());

        // Eine Datei, deren oeffentlicher Teil nicht zum geheimen passt,
        // wird abgewiesen statt still benutzt.
        QByteArray der = QByteArray::fromBase64(
            QByteArray(kVectorKeyPem).replace("-----BEGIN PRIVATE KEY-----", "")
                .replace("-----END PRIVATE KEY-----", "").replace('\n', ""));
        der[der.size() - 1] = char(der.at(der.size() - 1) ^ 0x01);
        const QByteArray bad = "-----BEGIN PRIVATE KEY-----\n" + der.toBase64()
                               + "\n-----END PRIVATE KEY-----\n";
        QVERIFY(!RoundtableKey::fromPkcs8Pem(bad).isValid());
    }

    void neuerSchluesselNurFuerDenBenutzer()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("sub/roundtable-identity.pem"));
        QString err;
        const RoundtableKey first = RoundtableKey::loadOrCreate(path, &err);
        QVERIFY2(first.isValid(), qPrintable(err));
        QVERIFY(QFile::exists(path));
#ifndef Q_OS_WIN
        QCOMPARE(QFile(path).permissions() & ~(QFileDevice::ReadUser | QFileDevice::WriteUser),
                 QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
        const QString id = first.roundtableId();
        QCOMPARE(id.size(), 26);

        // Beim zweiten Mal derselbe Schluessel.
        const RoundtableKey again = RoundtableKey::loadOrCreate(path, &err);
        QVERIFY2(again.isValid(), qPrintable(err));
        QCOMPARE(again.roundtableId(), id);

#ifndef Q_OS_WIN
        // Fuer andere lesbar gemacht: beim Laden wieder 0600.
        QVERIFY(QFile(path).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                           | QFileDevice::ReadGroup | QFileDevice::ReadOther));
        QVERIFY(RoundtableKey::loadOrCreate(path, &err).isValid());
        QCOMPARE(QFile(path).permissions()
                     & (QFileDevice::ReadGroup | QFileDevice::ReadOther),
                 QFileDevice::Permissions());
#endif

        // Zwei neue Schluessel sind verschieden.
        QVERIFY(RoundtableKey::generate().roundtableId() != RoundtableKey::generate().roundtableId());
    }
};

QTEST_GUILESS_MAIN(TstRoundtableKey)
#include "tst_roundtable_key.moc"
