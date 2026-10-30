// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_roundtable_wire.cpp  (NereusSDR)
// =================================================================
//
// Roundtable plan Task 10 (R-RT-1, the Core's runner of the conformance
// suite, docs/architecture/2026-10-23-roundtable-v1.md section 11):
//   - every control fixture (roundtable/conformance/v1/control/) decodes
//     or is refused as it says, by the table of its direction; one that
//     decodes keeps exactly the keys its kind lists, encodes again to the
//     same message, and, for what a station or live reader sends, is built
//     again by RoundtableWire's own message builders;
//   - the crypto vectors: strict base64url, the Roundtable id, the P-256
//     key check, and the sign-in signature cases; the transcript signed
//     with the fixture's test key verifies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-30: Guarded frequency-context and legacy command coverage.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10).
// =================================================================

#include "core/roundtable/RoundtableWire.h"
#include "core/security/StationIdentity.h"

#include <QFile>
#include <QFileDevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace NereusSDR;
namespace Wire = NereusSDR::RoundtableWire;

namespace {

const QString kSuite = QStringLiteral(NEREUS_ROUNDTABLE_CONFORMANCE_DIR);

// The fixtures that carry keys no kind lists (wire spec 12: version 1
// adds, so every receiver ignores what it does not know).
const QSet<QString> kExtraKeys{
    QStringLiteral("station-hello-extra-keys"),
    QStringLiteral("station-presence-key-station"),
    QStringLiteral("server-station-room-extra-keys"),
    QStringLiteral("server-station-welcome-extra-limits"),
};

// A number written with an exponent (the reading below holds no string
// with a digit in it).
const QRegularExpression kExponent(QStringLiteral("[0-9][eE][-+]?[0-9]"));

QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QJsonObject readObject(const QString& relative)
{
    return QJsonDocument::fromJson(readText(kSuite + QLatin1Char('/') + relative).toUtf8()).object();
}

// `decoded` holds only keys `wire` holds, with the same values.
bool within(const QJsonValue& decoded, const QJsonValue& wire)
{
    if (decoded.isObject()) {
        if (!wire.isObject()) {
            return false;
        }
        const QJsonObject d = decoded.toObject();
        const QJsonObject w = wire.toObject();
        for (auto it = d.constBegin(); it != d.constEnd(); ++it) {
            if (!w.contains(it.key()) || !within(it.value(), w.value(it.key()))) {
                return false;
            }
        }
        return true;
    }
    if (decoded.isArray()) {
        if (!wire.isArray() || wire.toArray().size() != decoded.toArray().size()) {
            return false;
        }
        for (qsizetype i = 0; i < decoded.toArray().size(); ++i) {
            if (!within(decoded.toArray().at(i), wire.toArray().at(i))) {
                return false;
            }
        }
        return true;
    }
    return decoded == wire;
}

QList<RoundtableSliceState> slicesOf(const QJsonArray& array)
{
    QList<RoundtableSliceState> slices;
    for (const QJsonValue& value : array) {
        const QJsonObject s = value.toObject();
        RoundtableSliceState slice;
        slice.id = s.value(QStringLiteral("id")).toInt();
        slice.hz = s.value(QStringLiteral("hz")).toInteger();
        slice.mode = s.value(QStringLiteral("mode")).toString();
        slice.filterHz = s.value(QStringLiteral("filterHz")).toInt();
        slice.keyed = s.value(QStringLiteral("keyed")).toBool();
        slices.append(slice);
    }
    return slices;
}

QString textOrEmpty(const QJsonValue& value)
{
    return value.isNull() ? QString() : value.toString();
}

// What a station or live reader sends, built again by the Core's builders
// from the decoded keys.
QJsonObject rebuild(const QJsonObject& m)
{
    const QString type = m.value(QStringLiteral("type")).toString();
    auto s = [&m](const char* key) { return m.value(QLatin1String(key)).toString(); };
    auto n = [&m](const char* key) { return m.value(QLatin1String(key)).toInteger(); };
    if (type == QLatin1String("hello")) {
        bool ok = false;
        if (m.contains(QStringLiteral("key"))) {
            return Wire::helloWithKey(s("software"), s("softwareVersion"), Wire::fromBase64Url(s("key"), &ok));
        }
        return Wire::hello(s("software"), s("softwareVersion"), Wire::fromBase64Url(s("spki"), &ok),
                           Wire::fromBase64Url(s("sig"), &ok));
    }
    if (type == QLatin1String("presence")) {
        return Wire::presence(textOrEmpty(m.value(QStringLiteral("callsign"))),
                              textOrEmpty(m.value(QStringLiteral("grid"))),
                              m.value(QStringLiteral("atRadio")).toBool(),
                              slicesOf(m.value(QStringLiteral("slices")).toArray()));
    }
    if (type == QLatin1String("reading")) {
        return Wire::reading(static_cast<int>(n("slice")), m.value(QStringLiteral("dbm")).toDouble(), n("t"),
                             m.value(QStringLiteral("present")).toBool());
    }
    if (type == QLatin1String("post")) {
        return Wire::post(s("room"), s("text"), s("clientId"));
    }
    if (type == QLatin1String("delete")) {
        return Wire::deleteMessage(n("msg"));
    }
    if (type == QLatin1String("name")) {
        return Wire::name(n("turn"), s("name"));
    }
    if (type == QLatin1String("report")) {
        const QString reason = textOrEmpty(m.value(QStringLiteral("reason")));
        const QString target = s("target");
        if (target == QLatin1String("message")) {
            return Wire::reportMessage(n("msg"), reason);
        }
        if (target == QLatin1String("turn")) {
            return Wire::reportTurn(n("turn"), reason);
        }
        return Wire::reportStation(s("room"), s("ref"), reason);
    }
    if (type == QLatin1String("read")) {
        return Wire::read(s("room"));
    }
    if (type == QLatin1String("unread")) {
        return Wire::unread(s("room"));
    }
    if (type == QLatin1String("resync")) {
        return Wire::resync(textOrEmpty(m.value(QStringLiteral("room"))));
    }
    if (type == QLatin1String("mentionsSeen")) {
        return Wire::mentionsSeen(n("t"));
    }
    if (type == QLatin1String("weblink")) {
        return Wire::weblink();
    }
    if (type == QLatin1String("deleteArchive")) {
        return Wire::deleteArchive(s("confirm"));
    }
    if (type == QLatin1String("hidden")) {
        return Wire::hidden();
    }
    if (type == QLatin1String("watchBand")) {
        return Wire::watchBand(s("band"));
    }
    if (type == QLatin1String("watch")) {
        return Wire::watch(s("room"));
    }
    if (type == QLatin1String("unwatch")) {
        return Wire::unwatch(s("room"));
    }
    return {};
}

bool directionOf(const QString& from, const QString& to, Wire::Direction* direction)
{
    if (from == QLatin1String("station")) {
        *direction = Wire::Direction::StationToServer;
    } else if (from == QLatin1String("live")) {
        *direction = Wire::Direction::LiveToServer;
    } else if (from == QLatin1String("server") && to == QLatin1String("station")) {
        *direction = Wire::Direction::ServerToStation;
    } else if (from == QLatin1String("server") && to == QLatin1String("live")) {
        *direction = Wire::Direction::ServerToLive;
    } else {
        return false;
    }
    return true;
}

// The fixture test key as a SEC1 private key file (the public point is
// derived on loading), so StationIdentity loads it as it would its own.
bool writeTestKey(const QString& directory, const QByteArray& privateKey)
{
    QByteArray der = QByteArray::fromHex("30310201010420");
    der.append(privateKey);
    der.append(QByteArray::fromHex("a00a06082a8648ce3d030107"));
    QByteArray pem("-----BEGIN EC PRIVATE KEY-----\n");
    pem.append(der.toBase64());
    pem.append("\n-----END EC PRIVATE KEY-----\n");
    QFile file(directory + QLatin1Char('/') + QLatin1String(StationIdentity::kKeyFileName));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(pem);
    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

} // namespace

class TstRoundtableWire : public QObject {
    Q_OBJECT

private slots:
    void controlFixtures_data();
    void controlFixtures();
    void everyKindHasAnAcceptedFixture();
    void refusalsTheParserCatches();
    void encoderWritesWholeNumbersAndNoExponent();
    void base64UrlVectors();
    void roundtableIdVectors();
    void p256SpkiVectors();
    void helloSignatureVectors();
    void transcriptSignedWithTheTestKeyVerifies();
    void callsignGridModeAndBandRules();
    void frequencyContextCommandsAreExplicitAndValidated();
    void historyIdsAndStatesAreBoundedAndStrict();
};

void TstRoundtableWire::controlFixtures_data()
{
    QTest::addColumn<QString>("file");
    const QJsonArray fixtures = readObject(QStringLiteral("manifest.json")).value(QStringLiteral("fixtures")).toArray();
    int count = 0;
    for (const QJsonValue& value : fixtures) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("kind")).toString() != QLatin1String("control")) {
            continue;
        }
        QTest::newRow(qPrintable(entry.value(QStringLiteral("id")).toString()))
            << entry.value(QStringLiteral("file")).toString();
        ++count;
    }
    QVERIFY2(count >= 160, "the manifest lists the control fixtures");
}

void TstRoundtableWire::controlFixtures()
{
    QFETCH(QString, file);
    const QString id = QFileInfo(file).completeBaseName();
    const QString source = readText(kSuite + QLatin1Char('/') + file);
    QVERIFY(!source.isEmpty());
    const QJsonObject fixture = QJsonDocument::fromJson(source.toUtf8()).object();
    const QString from = fixture.value(QStringLiteral("from")).toString();
    const QString to = fixture.value(QStringLiteral("to")).toString(QStringLiteral("server"));
    Wire::Direction direction{};
    QVERIFY(directionOf(from, to, &direction));

    // The wire as written in the file, so a `7.0` reaches the decoder as
    // `7.0` (QJsonDocument would write it back as `7`).
    const bool hasText = fixture.contains(QStringLiteral("wireText"));
    const QString text = hasText ? fixture.value(QStringLiteral("wireText")).toString()
                                 : Wire::memberSourceText(source, QStringLiteral("wire"));
    QVERIFY(!text.isEmpty());
    const bool decodes = fixture.value(QStringLiteral("decodes")).toBool();
    const Wire::Decoded decoded = Wire::decode(text, direction);
    QVERIFY2(decoded.ok == decodes, qPrintable(id + QStringLiteral(": ") + decoded.error));
    if (!decoded.ok) {
        return;
    }
    const QJsonObject wire = fixture.value(QStringLiteral("wire")).toObject();
    QVERIFY(within(decoded.message, wire));
    if (kExtraKeys.contains(id)) {
        QVERIFY2(decoded.message != wire, "an extra-keys fixture has a key its kind does not list");
    } else {
        QCOMPARE(decoded.message, wire);
    }

    // Encoded again, it decodes to the same message.
    const QByteArray encoded = Wire::encode(decoded.message);
    const Wire::Decoded again = Wire::decode(QString::fromUtf8(encoded), direction);
    QVERIFY2(again.ok, qPrintable(again.error));
    QCOMPARE(again.message, decoded.message);

    // What the Core sends, its builders make again.
    if (direction == Wire::Direction::StationToServer || direction == Wire::Direction::LiveToServer) {
        QCOMPARE(rebuild(decoded.message), decoded.message);
    }
}

void TstRoundtableWire::everyKindHasAnAcceptedFixture()
{
    // Every kind the Core sends or receives, in each direction it travels,
    // has a fixture that decodes (wire spec 11.3).
    const QHash<QString, QStringList> kinds{
        {QStringLiteral("station"),
         {QStringLiteral("hello"), QStringLiteral("presence"), QStringLiteral("reading"), QStringLiteral("post"),
          QStringLiteral("delete"), QStringLiteral("name"), QStringLiteral("report"), QStringLiteral("read"),
          QStringLiteral("unread"), QStringLiteral("resync"), QStringLiteral("mentionsSeen"),
          QStringLiteral("weblink"), QStringLiteral("deleteArchive"), QStringLiteral("hidden")}},
        {QStringLiteral("live"),
         {QStringLiteral("watchBand"), QStringLiteral("watch"), QStringLiteral("unwatch"), QStringLiteral("resync")}},
        {QStringLiteral("server-station"),
         {QStringLiteral("challenge"), QStringLiteral("welcome"), QStringLiteral("rooms"),
          QStringLiteral("roomsDelta"), QStringLiteral("room"), QStringLiteral("delta"), QStringLiteral("posted"),
          QStringLiteral("mention"), QStringLiteral("notice"), QStringLiteral("weblinkOk"), QStringLiteral("error")}},
        {QStringLiteral("server-live"),
         {QStringLiteral("rooms"), QStringLiteral("roomsDelta"), QStringLiteral("room"), QStringLiteral("delta"),
          QStringLiteral("error")}},
    };
    QSet<QString> accepted;
    const QJsonArray fixtures = readObject(QStringLiteral("manifest.json")).value(QStringLiteral("fixtures")).toArray();
    for (const QJsonValue& value : fixtures) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("kind")).toString() != QLatin1String("control")) {
            continue;
        }
        const QJsonObject fixture = readObject(entry.value(QStringLiteral("file")).toString());
        if (!fixture.value(QStringLiteral("decodes")).toBool()) {
            continue;
        }
        QString path = fixture.value(QStringLiteral("from")).toString();
        if (path == QLatin1String("server")) {
            path += QLatin1Char('-') + fixture.value(QStringLiteral("to")).toString();
        }
        accepted.insert(path + QLatin1Char(':')
                        + fixture.value(QStringLiteral("wire")).toObject().value(QStringLiteral("type")).toString());
    }
    for (auto it = kinds.cbegin(); it != kinds.cend(); ++it) {
        for (const QString& kind : it.value()) {
            QVERIFY2(accepted.contains(it.key() + QLatin1Char(':') + kind), qPrintable(it.key() + QLatin1Char(':') + kind));
        }
    }
}

void TstRoundtableWire::refusalsTheParserCatches()
{
    // Wire spec 5.1, the ones a fixture file cannot hold or the server
    // runner checks inline (roundtable/tests/test_control.py).
    const QStringList refused{
        QStringLiteral("not json"),
        QStringLiteral("[]"),
        QStringLiteral("{}"),
        QStringLiteral(R"({"type":7})"),
        QStringLiteral(R"({"type":"mentionsSeen","t":1,"t":2})"),
        QStringLiteral(R"({"type":"reading","slice":0,"dbm":NaN,"t":1,"present":true})"),
        QStringLiteral(R"({"type":"reading","slice":0,"dbm":-9e1,"t":1,"present":true})"),
        QStringLiteral(R"({"type":"mentionsSeen","t":1.0})"),
        QStringLiteral(R"({"type":"welcome"})"),
        QStringLiteral(R"({"type":"watch","room":"usb-14212000"})"),
        QStringLiteral(R"({"type":"post","room":"usb-14212000","text":"\ud800","clientId":"a"})"),
        QStringLiteral(R"({"type":"mentionsSeen","t":01})"),
        QStringLiteral(R"({"type":"mentionsSeen","t":1} x)"),
        QStringLiteral(R"({"type":"hidden","note":{"a":1,"a":2}})"),
    };
    for (const QString& text : refused) {
        QVERIFY2(!Wire::decode(text, Wire::Direction::StationToServer).ok, qPrintable(text));
    }
    // A lone surrogate, or an exponent, in a key no kind lists is ignored
    // with that key, at any depth.
    const Wire::Decoded unlisted = Wire::decode(
        QStringLiteral(R"({"type":"post","room":"usb-14212000","text":"hi","clientId":"a","note":{"deep":["\udc00",1e5]}})"),
        Wire::Direction::StationToServer);
    QVERIFY2(unlisted.ok, qPrintable(unlisted.error));
    QCOMPARE(unlisted.message,
             (QJsonObject{{QStringLiteral("type"), QStringLiteral("post")},
                          {QStringLiteral("room"), QStringLiteral("usb-14212000")},
                          {QStringLiteral("text"), QStringLiteral("hi")},
                          {QStringLiteral("clientId"), QStringLiteral("a")}}));
    // A surrogate pair is one character, not a lone surrogate.
    QVERIFY(Wire::decode(QStringLiteral(R"({"type":"post","room":"usb-14212000","text":"😀","clientId":"a"})"),
                         Wire::Direction::StationToServer)
                .ok);
}

void TstRoundtableWire::encoderWritesWholeNumbersAndNoExponent()
{
    const QByteArray reading = Wire::encode(Wire::reading(0, -101.0, 1800000000123, true));
    QCOMPARE(reading, QByteArray(R"({"type":"reading","dbm":-101,"present":true,"slice":0,"t":1800000000123})"));
    const QByteArray small = Wire::encode(Wire::reading(1, -0.0000125, 1, false));
    QVERIFY2(small.contains("\"dbm\":-0.0000125,"), small.constData());
    QVERIFY(!QString::fromUtf8(small).contains(kExponent));
    QVERIFY(Wire::decode(QString::fromUtf8(small), Wire::Direction::StationToServer).ok);
    const QByteArray text = Wire::encode(Wire::post(QStringLiteral("usb-14212000"),
                                                    QStringLiteral("73 de Köln \"q\"\n"),
                                                    QStringLiteral("c1")));
    QVERIFY(text.contains("K\xc3\xb6ln \\\"q\\\"\\n"));
}

void TstRoundtableWire::base64UrlVectors()
{
    const QJsonArray cases = readObject(QStringLiteral("crypto/base64url.json")).value(QStringLiteral("cases")).toArray();
    QVERIFY(!cases.isEmpty());
    for (const QJsonValue& value : cases) {
        const QJsonObject c = value.toObject();
        bool ok = false;
        const QByteArray bytes = Wire::fromBase64Url(c.value(QStringLiteral("text")).toString(), &ok);
        QVERIFY2(ok == c.value(QStringLiteral("valid")).toBool(), qPrintable(c.value(QStringLiteral("text")).toString()));
        if (ok) {
            QCOMPARE(bytes.toHex(), c.value(QStringLiteral("bytesHex")).toString().toLatin1());
        }
    }
}

void TstRoundtableWire::roundtableIdVectors()
{
    const QJsonObject vectors = readObject(QStringLiteral("crypto/roundtable-id.json"));
    QCOMPARE(QByteArray("NereusSDR roundtable id v1\n").toHex(), vectors.value(QStringLiteral("prefixHex")).toString().toLatin1());
    const QJsonArray cases = vectors.value(QStringLiteral("cases")).toArray();
    QVERIFY(!cases.isEmpty());
    for (const QJsonValue& value : cases) {
        const QJsonObject c = value.toObject();
        const QByteArray spki = QByteArray::fromHex(c.value(QStringLiteral("spkiHex")).toString().toLatin1());
        QCOMPARE(Wire::toBase64Url(spki), c.value(QStringLiteral("publicKey")).toString());
        QCOMPARE(Wire::roundtableId(spki), c.value(QStringLiteral("id")).toString());
        // Never the rendezvous id of the same key (design C20).
        QVERIFY(Wire::roundtableId(spki) != c.value(QStringLiteral("rendezvousId")).toString());
    }
}

void TstRoundtableWire::p256SpkiVectors()
{
    const QJsonArray cases = readObject(QStringLiteral("crypto/p256-spki.json")).value(QStringLiteral("cases")).toArray();
    QVERIFY(!cases.isEmpty());
    for (const QJsonValue& value : cases) {
        const QJsonObject c = value.toObject();
        const QByteArray spki = QByteArray::fromHex(c.value(QStringLiteral("spkiHex")).toString().toLatin1());
        QVERIFY2(StationIdentity::isP256Spki(spki) == c.value(QStringLiteral("valid")).toBool(),
                 qPrintable(c.value(QStringLiteral("name")).toString()));
    }
}

void TstRoundtableWire::helloSignatureVectors()
{
    const QJsonObject vectors = readObject(QStringLiteral("crypto/hello-signature.json"));
    bool ok = false;
    const QByteArray nonce = Wire::fromBase64Url(vectors.value(QStringLiteral("nonce")).toString(), &ok);
    QVERIFY(ok);
    const QByteArray transcript = Wire::helloTranscript(vectors.value(QStringLiteral("host")).toString(), nonce);
    QCOMPARE(transcript.toHex(), vectors.value(QStringLiteral("transcriptHex")).toString().toLatin1());
    QCOMPARE(transcript.size(), 87);
    QCOMPARE(Wire::signingHost(QUrl(QStringLiteral("wss://Roundtable.NereusSDR.com.:443/v1/station"))),
             QStringLiteral("roundtable.nereussdr.com"));
    const QJsonArray cases = vectors.value(QStringLiteral("cases")).toArray();
    QCOMPARE(cases.size(), 18);
    for (const QJsonValue& value : cases) {
        const QJsonObject c = value.toObject();
        bool keyOk = false;
        bool sigOk = false;
        const QByteArray spki = Wire::fromBase64Url(c.value(QStringLiteral("publicKey")).toString(), &keyOk);
        const QByteArray signature = Wire::fromBase64Url(c.value(QStringLiteral("signature")).toString(), &sigOk);
        const bool valid = keyOk && sigOk && signature.size() == StationIdentity::kSignatureBytes
            && StationIdentity::isP256Spki(spki) && StationIdentity::verify(spki, transcript, signature);
        QVERIFY2(valid == c.value(QStringLiteral("valid")).toBool(), qPrintable(c.value(QStringLiteral("name")).toString()));
    }
}

void TstRoundtableWire::transcriptSignedWithTheTestKeyVerifies()
{
    const QJsonObject vectors = readObject(QStringLiteral("crypto/hello-signature.json"));
    const QJsonObject key = vectors.value(QStringLiteral("key")).toObject();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeTestKey(dir.path(), QByteArray::fromHex(key.value(QStringLiteral("privateKeyHex")).toString().toLatin1())));
    const StationIdentity identity = StationIdentity::loadOrCreate(dir.path());
    QVERIFY2(identity.isValid(), qPrintable(identity.lastError()));
    QVERIFY(!identity.wasCreatedThisRun());
    QCOMPARE(Wire::toBase64Url(identity.publicKeySpki()), key.value(QStringLiteral("publicKey")).toString());
    QCOMPARE(Wire::roundtableId(identity.publicKeySpki()), key.value(QStringLiteral("id")).toString());

    bool ok = false;
    const QByteArray nonce = Wire::fromBase64Url(vectors.value(QStringLiteral("nonce")).toString(), &ok);
    const QByteArray transcript = Wire::helloTranscript(vectors.value(QStringLiteral("host")).toString(), nonce);
    const QByteArray signature = identity.sign(transcript);
    QCOMPARE(signature.size(), StationIdentity::kSignatureBytes);
    QVERIFY(StationIdentity::verify(identity.publicKeySpki(), transcript, signature));
    // The hello the Core sends with it decodes.
    const QJsonObject hello = Wire::hello(QStringLiteral("NereusSDR"), QStringLiteral("26.10.3"),
                                          identity.publicKeySpki(), signature);
    QVERIFY(Wire::decode(QString::fromUtf8(Wire::encode(hello)), Wire::Direction::StationToServer).ok);
    // Another host's transcript does not verify.
    QVERIFY(!StationIdentity::verify(identity.publicKeySpki(),
                                     Wire::helloTranscript(QStringLiteral("roundtable.example.invalid"), nonce),
                                     signature));
}

void TstRoundtableWire::callsignGridModeAndBandRules()
{
    // Wire spec 4.5.
    for (const char* call : {"KG4VCF", "VE3/W4XYZ/P", "w4xyz", "K1A"}) {
        QVERIFY2(Wire::isCallsign(QLatin1String(call)), call);
    }
    for (const char* call : {"", "K1", "KG4-VCF", "KG4 VCF", "/KG4VCF", "KG4VCF/", "KG4//VCF", "ABCDEF",
                             "123456", "KG4VCFKG4VCFKG4VC"}) {
        QVERIFY2(!Wire::isCallsign(QLatin1String(call)), call);
    }
    QCOMPARE(Wire::baseCallsign(QStringLiteral("VE3/W4XYZ/P")), QStringLiteral("W4XYZ"));
    QCOMPARE(Wire::baseCallsign(QStringLiteral("ab12/cd34")), QStringLiteral("AB12"));
    QCOMPARE(Wire::gridToSend(QStringLiteral("em73TU42")), QStringLiteral("EM73tu"));
    QCOMPARE(Wire::gridToSend(QStringLiteral("EM73")), QStringLiteral("EM73"));
    QCOMPARE(Wire::gridToSend(QStringLiteral("EM")), QString());
    QCOMPARE(Wire::gridToSend(QStringLiteral("EM73tu42xx")), QString());
    QCOMPARE(Wire::gridToSend(QStringLiteral("ZZ73")), QString());
    QCOMPARE(Wire::gridToSend(QString()), QString());
    // 6.1 and 6.2.
    QCOMPARE(Wire::familyOfMode(QStringLiteral("SAM")), QStringLiteral("am"));
    QCOMPARE(Wire::familyOfMode(QStringLiteral("CWU")), QStringLiteral("cw"));
    QCOMPARE(Wire::familyOfMode(QStringLiteral("DSB")), QString());
    QCOMPARE(Wire::bandOf(14212000), QStringLiteral("20m"));
    QCOMPARE(Wire::bandOf(135700), QStringLiteral("2200m"));
    QCOMPARE(Wire::bandOf(1300000000), QStringLiteral("23cm"));
    QCOMPARE(Wire::bandOf(14350001), QStringLiteral("other"));
    // 6.10: words with an optional @, by base callsign.
    QVERIFY(Wire::mentionedBaseCallsigns(QStringLiteral("@kg4vcf/m are you copying K4ABC?"))
                .contains(QStringLiteral("KG4VCF")));
}

void TstRoundtableWire::frequencyContextCommandsAreExplicitAndValidated()
{
    const QJsonObject post = Wire::postAt(1, 3963007, QStringLiteral("lsb"),
                                        QStringLiteral("Hello"), QStringLiteral("frequency-test"));
    const auto decoded = Wire::decode(QString::fromUtf8(Wire::encode(post)), Wire::Direction::StationToServer);
    QVERIFY2(decoded.ok, qPrintable(decoded.error));
    QCOMPARE(decoded.message, post);
    QCOMPARE(decoded.message.value(QStringLiteral("hz")).toInteger(), qint64(3963007));
    QJsonObject incomplete = post;
    incomplete.remove(QStringLiteral("hz"));
    QVERIFY(!Wire::decode(QString::fromUtf8(Wire::encode(incomplete)), Wire::Direction::StationToServer).ok);
    incomplete = post;
    incomplete.insert(QStringLiteral("family"), QStringLiteral("room"));
    QVERIFY(!Wire::decode(QString::fromUtf8(Wire::encode(incomplete)), Wire::Direction::StationToServer).ok);
    for (const bool live : {false, true}) {
        const QJsonObject read = Wire::readAt(3963007, QStringLiteral("lsb"), live);
        const auto result = Wire::decode(QString::fromUtf8(Wire::encode(read)), live ? Wire::Direction::LiveToServer
                                                               : Wire::Direction::StationToServer);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.message, read);
    }
    // Existing builders retain the exact old command, with no implicit retargeting.
    const QJsonObject legacy = Wire::post(QStringLiteral("lsb-3963000"),
                                         QStringLiteral("Hello"), QStringLiteral("legacy"));
    QVERIFY(!legacy.contains(QStringLiteral("hz")));
    QCOMPARE(legacy.value(QStringLiteral("type")).toString(), QStringLiteral("post"));
}

QTEST_GUILESS_MAIN(TstRoundtableWire)
#include "tst_roundtable_wire.moc"

void TstRoundtableWire::historyIdsAndStatesAreBoundedAndStrict()
{
    for (const auto direction : {Wire::Direction::StationToServer, Wire::Direction::LiveToServer}) {
        QVERIFY(Wire::decode(QStringLiteral(R"({"type":"history","clientId":"h1","ids":[]})"), direction).ok);
        QVERIFY(Wire::decode(QStringLiteral(R"({"type":"history","clientId":"h1","ids":[1,2]})"), direction).ok);
        for (const QString& ids : {QStringLiteral("[true]"), QStringLiteral("[1.0]"),
                QStringLiteral("[1e0]"), QStringLiteral("[0]"), QStringLiteral("[-1]"),
                QStringLiteral("[1,1]")}) {
            QVERIFY(!Wire::decode(QStringLiteral(R"({"type":"history","clientId":"h1","ids":%1})").arg(ids), direction).ok);
        }
        QJsonArray ids;
        for (int i = 1; i <= 200; ++i) { ids.append(i); }
        QJsonObject request{{"type", "history"}, {"clientId", "h1"}, {"ids", ids}};
        QVERIFY(Wire::decode(QString::fromUtf8(Wire::encode(request)), direction).ok);
        ids.append(201);
        request.insert("ids", ids);
        QVERIFY(!Wire::decode(QString::fromUtf8(Wire::encode(request)), direction).ok);
    }
    for (const auto direction : {Wire::Direction::ServerToStation, Wire::Direction::ServerToLive}) {
        const QString good = QStringLiteral(R"({"type":"history","clientId":"h1","states":[{"id":1,"state":"shown","text":"retained"},{"id":2,"state":"expired","text":null}]})");
        QVERIFY(Wire::decode(good, direction).ok);
        for (const QString& bad : {QString(good).replace(QStringLiteral("\"expired\""), QStringLiteral("\"other\"")),
                QStringLiteral(R"({"type":"history","clientId":"h1","states":[{"id":1,"state":"deleted","text":"leak"}]})"),
                QStringLiteral(R"({"type":"history","clientId":"h1","states":[{"id":1,"state":"shown","text":null}]})"),
                QStringLiteral(R"({"type":"history","clientId":"h1","states":[{"id":1,"state":"expired","text":null},{"id":1,"state":"expired","text":null}]})")}) {
            QVERIFY(!Wire::decode(bad, direction).ok);
        }
    }
}
