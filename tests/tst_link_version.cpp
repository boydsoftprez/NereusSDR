// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_version.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 4 (R-IOS-01; spec D23 and D39): link versions both
// ways and the hello's declared features.
//
// The invariant table first: agreeMajor() over every case the plan names
// and the refusal wording. Then the hello codec (today's hello decodes as
// it always did; the new keys are optional and checked), then the two
// ends negotiating with injected supported lists, because a second major
// does not exist yet: the station accepts a client major from its own list
// and refuses any other in plain words; the desktop client picks the
// highest shared major or leaves without retrying. Then the declared
// features, the --test-link-majors option and the station's TLS minimum.
//
// Loopback transports only; no radio, no station computer, no real audio.
//
//   cmake --build build --target tst_link_version
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_link_version$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): link
//                                    version negotiation and declared
//                                    features. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    a client built before `majors` is
//                                    served by a station that still
//                                    accepts its major; the empty-majors
//                                    guard; a TLS 1.1 handshake refused.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 completion carry, review I1
//                                    (R-R3-38, R-IOS-01): the refusal worded
//                                    with "Core" by SessionEndReasons; a
//                                    client that refuses records the
//                                    version end. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSslConfiguration>
#include <QSslServer>
#include <QSslSocket>
#include <QTemporaryDir>

#include <algorithm>
#include <optional>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/session/LinkVersion.h"
#include "core/session/SessionEndReasons.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "models/RadioModel.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

using MajorList = QList<quint16>;

/// The settings schema this run's store is at (initTestCase), sent in a
/// fake station's hello so the client sees no skew.
constexpr qint32 kStationSchema = 1;
Q_DECLARE_METATYPE(MajorList)

namespace {

QJsonObject wireObject(const QByteArray& wire)
{
    return QJsonDocument::fromJson(wire).object();
}

/// The last message of `type` in `received`, or an empty object.
QJsonObject lastOfType(const QList<QByteArray>& received, const QString& type)
{
    for (auto it = received.crbegin(); it != received.crend(); ++it) {
        const QJsonObject o = wireObject(*it);
        if (o.value(QStringLiteral("type")).toString() == type) {
            return o;
        }
    }
    return {};
}

MajorList majorsOf(const QJsonObject& hello)
{
    MajorList majors;
    for (const QJsonValue& v : hello.value(QStringLiteral("majors")).toArray()) {
        majors.append(quint16(v.toInt()));
    }
    return majors;
}

/// A client hello as an app sends it: `major` is the one it chose, and
/// `majors` (when not empty) the ones it supports.
QByteArray clientHello(quint16 major, const MajorList& majors,
                       const QHash<QByteArray, int>& features = {})
{
    const SessionMessage hello =
        majors.isEmpty()
            ? SessionMessages::hello(major, kSessionProtocolMinor, 0,
                                     QStringLiteral("NereusSDR iPhone"))
            : SessionMessages::hello(major, kSessionProtocolMinor, 0,
                                     QStringLiteral("NereusSDR iPhone"), majors, features);
    return SessionMessages::encode(hello);
}

} // namespace

class TstLinkVersion : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ---- The invariant table ----
    void agreeMajorTable_data();
    void agreeMajorTable();
    void supportedMajorsAreThisMajorAndTheOneBefore();
    void refusalNamesBothSidesInPlainWords_data();
    void refusalNamesBothSidesInPlainWords();

    // ---- The hello ----
    void helloCarriesMajorsAndFeatures();
    void helloWithoutTheNewKeysIsTodaysHello();
    void helloIgnoresUnknownFeatures();
    void helloBuilderNeverSendsAnEmptyMajors();
    void helloRefusesMalformedDeclarations_data();
    void helloRefusesMalformedDeclarations();

    // ---- The station ----
    void stationHelloDeclaresItsMajorsAndFeatures();
    void stationNegotiation_data();
    void stationNegotiation();
    void peerDeclaresWhatItsHelloDeclared();
    void aClientBuiltBeforeMajorsIsServedByATwoMajorStation_data();
    void aClientBuiltBeforeMajorsIsServedByATwoMajorStation();

    // ---- The desktop client ----
    void clientNegotiation_data();
    void clientNegotiation();
    void stationDeclaresWhatItsHelloDeclared();

    // ---- nereusd's --test-link-majors ----
    void testLinkMajorsOption();

    // ---- TLS ----
    void stationTlsMinimumIsTls12OrLater();
    void stationRefusesATls11Handshake();

private:
    QTemporaryDir m_securityDir;
};

void TstLinkVersion::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    qRegisterMetaType<MajorList>();
    // A settings store of this run's own, already at a schema version, as
    // CoreInit leaves it before a window builds its StationClient.
    AppSettings::setProfileOverride(
        QStringLiteral("link-version-%1").arg(QCoreApplication::applicationPid()));
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                     QStringLiteral("1"));
}

void TstLinkVersion::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

// ── The invariant table ──────────────────────────────────────────────────

void TstLinkVersion::agreeMajorTable_data()
{
    QTest::addColumn<MajorList>("ours");
    QTest::addColumn<MajorList>("theirs");
    QTest::addColumn<int>("agreed"); // 0: none

    // The plan's five cases.
    QTest::newRow("{1} {1}") << MajorList{1} << MajorList{1} << 1;
    QTest::newRow("{1,2} {2,3}") << MajorList{1, 2} << MajorList{2, 3} << 2;
    QTest::newRow("{2,3} {1}") << MajorList{2, 3} << MajorList{1} << 0;
    QTest::newRow("{1} {2,3}") << MajorList{1} << MajorList{2, 3} << 0;
    QTest::newRow("{3,4} {2,3}") << MajorList{3, 4} << MajorList{2, 3} << 3;
    // One major apart is always agreed (D23); two apart never is (D39).
    QTest::newRow("{1,2} {1,2}") << MajorList{1, 2} << MajorList{1, 2} << 2;
    QTest::newRow("{1,2} {2}") << MajorList{1, 2} << MajorList{2} << 2;
    QTest::newRow("{2} {1,2}") << MajorList{2} << MajorList{1, 2} << 2;
    QTest::newRow("{1} {3}") << MajorList{1} << MajorList{3} << 0;
    // Order within a list does not matter.
    QTest::newRow("{2,1} {3,2}") << MajorList{2, 1} << MajorList{3, 2} << 2;
    QTest::newRow("empty") << MajorList{} << MajorList{1} << 0;
}

void TstLinkVersion::agreeMajorTable()
{
    QFETCH(MajorList, ours);
    QFETCH(MajorList, theirs);
    QFETCH(int, agreed);
    const std::optional<quint16> result = LinkVersion::agreeMajor(ours, theirs);
    if (agreed == 0) {
        QVERIFY(!result.has_value());
    } else {
        QVERIFY(result.has_value());
        QCOMPARE(int(*result), agreed);
    }
    // Symmetric: both ends reach the same answer.
    QCOMPARE(LinkVersion::agreeMajor(theirs, ours), result);
}

void TstLinkVersion::supportedMajorsAreThisMajorAndTheOneBefore()
{
    const MajorList majors = LinkVersion::supportedMajors();
    QCOMPARE(majors, (MajorList{1}));
    QCOMPARE(majors.last(), kSessionProtocolMajor);
    QVERIFY(majors.size() <= 2);
    if (majors.size() == 2) {
        QCOMPARE(int(majors.first()) + 1, int(majors.last()));
    }
    // The version rule: the minor does not move.
    QCOMPARE(kSessionProtocolMinor, quint16(11));
}

void TstLinkVersion::refusalNamesBothSidesInPlainWords_data()
{
    QTest::addColumn<MajorList>("station");
    QTest::addColumn<MajorList>("client");
    QTest::addColumn<QString>("expected");

    QTest::newRow("Core older")
        << MajorList{1} << MajorList{2, 3}
        << QStringLiteral("This Core runs link version 1 and this app runs version 3. "
                          "Update the Core.");
    QTest::newRow("app older")
        << MajorList{2, 3} << MajorList{1}
        << QStringLiteral("This Core runs link version 3 and this app runs version 1. "
                          "Update this app.");
    QTest::newRow("app with one major")
        << MajorList{1} << MajorList{3}
        << QStringLiteral("This Core runs link version 1 and this app runs version 3. "
                          "Update the Core.");
}

void TstLinkVersion::refusalNamesBothSidesInPlainWords()
{
    QFETCH(MajorList, station);
    QFETCH(MajorList, client);
    QFETCH(QString, expected);
    const QString text = SessionEndReasons::versionRefused(station, client);
    QCOMPARE(text, expected);
    QVERIFY2(OperatorWording::isPlain(text), qPrintable(OperatorWording::internalTermIn(text)));
}

// ── The hello ────────────────────────────────────────────────────────────

void TstLinkVersion::helloCarriesMajorsAndFeatures()
{
    const QHash<QByteArray, int> features{{"deviceAuth", 1}, {"pairing", 2}};
    const QByteArray wire = SessionMessages::encode(SessionMessages::hello(
        2, 11, 0, QStringLiteral("nereusd"), MajorList{1, 2}, features));
    const QJsonObject o = wireObject(wire);
    QCOMPARE(majorsOf(o), (MajorList{1, 2}));
    const QJsonObject declared = o.value(QStringLiteral("features")).toObject();
    QCOMPARE(declared.size(), 2);
    QCOMPARE(declared.value(QStringLiteral("deviceAuth")).toInt(), 1);
    QCOMPARE(declared.value(QStringLiteral("pairing")).toInt(), 2);

    SessionMessage decoded;
    QVERIFY(SessionMessages::decode(wire, &decoded));
    QCOMPARE(decoded.protocolMajor, quint16(2));
    QCOMPARE(decoded.supportedMajors, (MajorList{1, 2}));
    QCOMPARE(decoded.features, features);
    // Encoding the decoded hello again gives the same message.
    QCOMPARE(wireObject(SessionMessages::encode(decoded)), o);

    // An empty feature list is still sent: the station's hello always
    // carries both keys.
    const QJsonObject bare = wireObject(SessionMessages::encode(
        SessionMessages::hello(1, 11, 0, QStringLiteral("nereusd"), MajorList{1}, {})));
    QVERIFY(bare.contains(QStringLiteral("features")));
    QVERIFY(bare.value(QStringLiteral("features")).toObject().isEmpty());
}

void TstLinkVersion::helloWithoutTheNewKeysIsTodaysHello()
{
    // What an older peer sends, byte for byte as today.
    const QByteArray today =
        R"({"type":"hello","major":1,"minor":11,"settingsSchema":0,"peer":"older app"})";
    SessionMessage decoded;
    QVERIFY(SessionMessages::decode(today, &decoded));
    QCOMPARE(decoded.protocolMajor, quint16(1));
    // Absent majors means [major]; absent features means none.
    QCOMPARE(decoded.supportedMajors, (MajorList{1}));
    QVERIFY(decoded.features.isEmpty());
    // Encoded again it is still today's hello: no key appears.
    const QJsonObject again = wireObject(SessionMessages::encode(decoded));
    QCOMPARE(again, wireObject(today));

    // The four-argument builder is today's hello too.
    const QJsonObject built = wireObject(SessionMessages::encode(
        SessionMessages::hello(1, 11, 0, QStringLiteral("older app"))));
    QVERIFY(!built.contains(QStringLiteral("majors")));
    QVERIFY(!built.contains(QStringLiteral("features")));

    // A hello from a major-3 peer with no list stands for [3].
    SessionMessage three;
    QVERIFY(SessionMessages::decode(
        R"({"type":"hello","major":3,"minor":0,"settingsSchema":0,"peer":"future"})", &three));
    QCOMPARE(three.supportedMajors, (MajorList{3}));
}

void TstLinkVersion::helloIgnoresUnknownFeatures()
{
    SessionMessage decoded;
    QVERIFY(SessionMessages::decode(
        R"({"type":"hello","major":1,"minor":11,"settingsSchema":0,"peer":"app",)"
        R"("majors":[1],"features":{"deviceAuth":1,"somethingNotYetInvented":7}})",
        &decoded));
    QCOMPARE(decoded.features.value("deviceAuth"), 1);
    QCOMPARE(decoded.features.value("somethingNotYetInvented"), 7);
}

void TstLinkVersion::helloBuilderNeverSendsAnEmptyMajors()
{
    // `majors` is never empty on the wire (decode refuses []). Given an
    // empty list, the builder sends what an absent key means: [major].
    const QByteArray wire = SessionMessages::encode(
        SessionMessages::hello(1, 11, 0, QStringLiteral("nereusd"), MajorList{}, {}));
    QCOMPARE(majorsOf(wireObject(wire)), (MajorList{1}));
    SessionMessage decoded;
    QVERIFY(SessionMessages::decode(wire, &decoded));
    QCOMPARE(decoded.supportedMajors, (MajorList{1}));
}

void TstLinkVersion::helloRefusesMalformedDeclarations_data()
{
    QTest::addColumn<QByteArray>("tail");
    QTest::newRow("majors not an array") << QByteArray(R"("majors":1)");
    QTest::newRow("majors empty") << QByteArray(R"("majors":[])");
    QTest::newRow("major not whole") << QByteArray(R"("majors":[1.5])");
    QTest::newRow("major a string") << QByteArray(R"("majors":["1"])");
    QTest::newRow("major out of range") << QByteArray(R"("majors":[65536])");
    QTest::newRow("major negative") << QByteArray(R"("majors":[-1])");
    QTest::newRow("features not an object") << QByteArray(R"("features":[])");
    QTest::newRow("feature version not whole") << QByteArray(R"("features":{"a":1.5})");
    QTest::newRow("feature version a string") << QByteArray(R"("features":{"a":"1"})");
    QTest::newRow("feature version negative") << QByteArray(R"("features":{"a":-1})");
    QTest::newRow("feature version too large")
        << QByteArray(R"("features":{"a":2147483648})");
    QTest::newRow("feature name empty") << QByteArray(R"("features":{"":1})");
}

void TstLinkVersion::helloRefusesMalformedDeclarations()
{
    QFETCH(QByteArray, tail);
    const QByteArray wire =
        QByteArray(R"({"type":"hello","major":1,"minor":11,"settingsSchema":0,"peer":"app",)")
        + tail + "}";
    SessionMessage decoded;
    QVERIFY2(!SessionMessages::decode(wire, &decoded), wire.constData());
}

// ── The station ──────────────────────────────────────────────────────────

void TstLinkVersion::stationHelloDeclaresItsMajorsAndFeatures()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
    RadioModel model;

    {
        StationServer server(&model, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        QCOMPARE(server.supportedMajors(), (MajorList{1}));
        auto* station = new LoopbackTransport(QStringLiteral("station"), this);
        auto* app = new LoopbackTransport(QStringLiteral("app"), this);
        station->linkTo(app);
        server.acceptTransport(station);
        QTRY_VERIFY(!lastOfType(app->received(), QStringLiteral("hello")).isEmpty());
        const QJsonObject hello = lastOfType(app->received(), QStringLiteral("hello"));
        QCOMPARE(hello.value(QStringLiteral("major")).toInt(), 1);
        QCOMPARE(hello.value(QStringLiteral("minor")).toInt(), int(kSessionProtocolMinor));
        QCOMPARE(majorsOf(hello), (MajorList{1}));
        QVERIFY(hello.value(QStringLiteral("features")).isObject());
        // iPhone app Task 12: device sign-in; Task 14: pairing.
        QCOMPARE(hello.value(QStringLiteral("features")).toObject(),
                 (QJsonObject{{QStringLiteral("deviceAuth"), 1},
                              {QStringLiteral("pairing"), 1},
                              // iPhone app Task 71: several devices at once.
                              {QStringLiteral("sessionHolder"), 1}}));
    }
    {
        // Injected: the station's hello names its oldest major, which a
        // client built before `majors` existed reads, and the list.
        StationServer server(&model, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()), nullptr, MajorList{1, 2});
        auto* station = new LoopbackTransport(QStringLiteral("station"), this);
        auto* app = new LoopbackTransport(QStringLiteral("app"), this);
        station->linkTo(app);
        server.acceptTransport(station);
        QTRY_VERIFY(!lastOfType(app->received(), QStringLiteral("hello")).isEmpty());
        const QJsonObject hello = lastOfType(app->received(), QStringLiteral("hello"));
        QCOMPARE(hello.value(QStringLiteral("major")).toInt(), 1);
        QCOMPARE(majorsOf(hello), (MajorList{1, 2}));
    }
}

void TstLinkVersion::stationNegotiation_data()
{
    QTest::addColumn<MajorList>("stationMajors");
    QTest::addColumn<int>("clientMajor");
    QTest::addColumn<MajorList>("clientMajors"); // empty: an older app's hello
    QTest::addColumn<int>("agreed");             // 0: refused

    QTest::newRow("older app, same major") << MajorList{1} << 1 << MajorList{} << 1;
    QTest::newRow("same major") << MajorList{1} << 1 << MajorList{1} << 1;
    QTest::newRow("app one ahead") << MajorList{1, 2} << 2 << MajorList{2, 3} << 2;
    QTest::newRow("app one behind") << MajorList{2, 3} << 2 << MajorList{1, 2} << 2;
    QTest::newRow("app chose the older shared") << MajorList{1, 2} << 1 << MajorList{1} << 1;
    QTest::newRow("app two behind") << MajorList{2, 3} << 1 << MajorList{1} << 0;
    QTest::newRow("app two ahead") << MajorList{1} << 3 << MajorList{2, 3} << 0;
    QTest::newRow("older app, other major") << MajorList{1} << 2 << MajorList{} << 0;
}

void TstLinkVersion::stationNegotiation()
{
    QFETCH(MajorList, stationMajors);
    QFETCH(int, clientMajor);
    QFETCH(MajorList, clientMajors);
    QFETCH(int, agreed);

    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
    RadioModel model;
    StationServer server(&model, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()), nullptr, stationMajors);
    auto* station = new LoopbackTransport(QStringLiteral("station"), this);
    auto* app = new LoopbackTransport(QStringLiteral("app"), this);
    station->linkTo(app);
    server.acceptTransport(station);

    if (agreed == 0) {
        QTest::ignoreMessage(
            QtWarningMsg, QRegularExpression(QStringLiteral("^Refusing a client on link major")));
    }
    app->sendText(clientHello(quint16(clientMajor), clientMajors));
    app->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));

    if (agreed != 0) {
        QTRY_VERIFY(server.hasAuthenticatedSession());
        QCOMPARE(int(server.peerAgreedMajor(station)), agreed);
        QTRY_VERIFY(!lastOfType(app->received(), QStringLiteral("auth.result")).isEmpty());
        const QJsonObject result = lastOfType(app->received(), QStringLiteral("auth.result"));
        QVERIFY(result.value(QStringLiteral("accepted")).toBool());
        return;
    }

    QTRY_VERIFY(!app->isOpen());
    QVERIFY(!server.hasAuthenticatedSession());
    // Refused before authentication: no auth.result, one session.end.
    QVERIFY(lastOfType(app->received(), QStringLiteral("auth.result")).isEmpty());
    const QJsonObject end = lastOfType(app->received(), QStringLiteral("session.end"));
    QVERIFY(!end.isEmpty());
    QCOMPARE(end.value(QStringLiteral("retryable")).toBool(true), false);
    const MajorList named = clientMajors.isEmpty() ? MajorList{quint16(clientMajor)}
                                                   : clientMajors;
    const QString reason = end.value(QStringLiteral("reason")).toString();
    QCOMPARE(reason, SessionEndReasons::versionRefused(stationMajors, named));
    QVERIFY(OperatorWording::isPlain(reason));
}

void TstLinkVersion::peerDeclaresWhatItsHelloDeclared()
{
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
    RadioModel model;
    StationServer server(&model, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

    auto* station = new LoopbackTransport(QStringLiteral("station"), this);
    auto* app = new LoopbackTransport(QStringLiteral("app"), this);
    station->linkTo(app);
    server.acceptTransport(station);
    QVERIFY(!server.peerDeclares(station, "deviceAuth", 1));
    app->sendText(clientHello(1, MajorList{1},
                              {{"deviceAuth", 2}, {"somethingNotYetInvented", 9}}));
    QTRY_VERIFY(server.peerDeclares(station, "deviceAuth", 1));
    QVERIFY(server.peerDeclares(station, "deviceAuth", 2));
    QVERIFY(!server.peerDeclares(station, "deviceAuth", 3));
    QVERIFY(!server.peerDeclares(station, "pairing", 1));
    QVERIFY(!server.peerDeclares(nullptr, "deviceAuth", 1));

    // An older app declares nothing.
    auto* olderStation = new LoopbackTransport(QStringLiteral("older-station"), this);
    auto* olderApp = new LoopbackTransport(QStringLiteral("older-app"), this);
    olderStation->linkTo(olderApp);
    server.acceptTransport(olderStation);
    olderApp->sendText(clientHello(1, MajorList{}));
    QTRY_COMPARE(int(server.peerAgreedMajor(olderStation)), 1);
    QVERIFY(!server.peerDeclares(olderStation, "deviceAuth", 1));
}

void TstLinkVersion::aClientBuiltBeforeMajorsIsServedByATwoMajorStation_data()
{
    QTest::addColumn<MajorList>("stationMajors");
    QTest::addColumn<bool>("served");

    QTest::newRow("station [1]") << MajorList{1} << true;
    QTest::newRow("station [1, 2]") << MajorList{1, 2} << true;
    QTest::newRow("station [2, 3]") << MajorList{2, 3} << false;
}

void TstLinkVersion::aClientBuiltBeforeMajorsIsServedByATwoMajorStation()
{
    // A client built before `majors` existed (before 4cd6a4d1) reads only
    // the station hello's `major`, and leaves unless it equals the one
    // major it speaks, 1 (StationClient::handleHello at 4cd6a4d1^:
    // `message.protocolMajor != kSessionProtocolMajor` disconnects). A
    // station that still accepts major 1 must therefore send 1 there, or
    // it turns away every such client it could serve (spec D39).
    QFETCH(MajorList, stationMajors);
    QFETCH(bool, served);

    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
    RadioModel model;
    StationServer server(&model, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()), nullptr, stationMajors);
    auto* station = new LoopbackTransport(QStringLiteral("station"), this);
    auto* app = new LoopbackTransport(QStringLiteral("app"), this);
    station->linkTo(app);
    server.acceptTransport(station);
    QTRY_VERIFY(!app->received().isEmpty());

    SessionMessage hello;
    QVERIFY(SessionMessages::decode(app->received().first(), &hello));
    QCOMPARE(hello.kind, SessionMessageKind::Hello);
    constexpr quint16 kOnlyMajorOfAnOlderClient = 1;
    const bool olderClientStays = hello.protocolMajor == kOnlyMajorOfAnOlderClient;
    QCOMPARE(olderClientStays, served);
    if (!served) {
        return;
    }

    // It stays, and answers as it always has: no `majors`, no `features`.
    app->sendText(clientHello(kOnlyMajorOfAnOlderClient, MajorList{}));
    app->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    QTRY_VERIFY(server.hasAuthenticatedSession());
    QCOMPARE(int(server.peerAgreedMajor(station)), 1);
}

// ── The desktop client ───────────────────────────────────────────────────

void TstLinkVersion::clientNegotiation_data()
{
    QTest::addColumn<MajorList>("clientMajors");
    // A station's hello names the oldest of its majors in `major` (the
    // link document's section 6.1); an older station's names its only one.
    QTest::addColumn<int>("stationMajor");
    QTest::addColumn<MajorList>("stationMajors"); // empty: an older station's hello
    QTest::addColumn<int>("sent");                // 0: leaves without sending

    QTest::newRow("older station, same major") << MajorList{1} << 1 << MajorList{} << 1;
    QTest::newRow("same major") << MajorList{1} << 1 << MajorList{1} << 1;
    QTest::newRow("station one ahead") << MajorList{1, 2} << 2 << MajorList{2, 3} << 2;
    QTest::newRow("station one behind") << MajorList{2, 3} << 1 << MajorList{1, 2} << 2;
    QTest::newRow("older station one behind") << MajorList{1, 2} << 1 << MajorList{} << 1;
    QTest::newRow("station two ahead") << MajorList{1} << 2 << MajorList{2, 3} << 0;
    QTest::newRow("station two behind") << MajorList{2, 3} << 1 << MajorList{} << 0;
}

void TstLinkVersion::clientNegotiation()
{
    QFETCH(MajorList, clientMajors);
    QFETCH(int, stationMajor);
    QFETCH(MajorList, stationMajors);
    QFETCH(int, sent);

    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy, nullptr, clientMajors);
    QSignalSpy retries(&client, &StationClient::reconnectScheduled);
    auto* station = new LoopbackTransport(QStringLiteral("fake-station"), this);
    auto* app = new LoopbackTransport(QStringLiteral("desktop"), this);
    station->linkTo(app);
    client.startSession(app, QStringLiteral("not-a-real-token"));
    if (sent == 0) {
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^No link major shared")));
    }

    const SessionMessage hello =
        stationMajors.isEmpty()
            ? SessionMessages::hello(quint16(stationMajor), kSessionProtocolMinor, kStationSchema,
                                     QStringLiteral("fake-station"))
            : SessionMessages::hello(quint16(stationMajor), kSessionProtocolMinor, kStationSchema,
                                     QStringLiteral("fake-station"), stationMajors, {});
    station->sendText(SessionMessages::encode(hello));

    if (sent != 0) {
        QTRY_VERIFY(!lastOfType(station->received(), QStringLiteral("auth.request")).isEmpty());
        const QJsonObject reply = lastOfType(station->received(), QStringLiteral("hello"));
        QCOMPARE(reply.value(QStringLiteral("major")).toInt(), sent);
        QCOMPARE(majorsOf(reply), clientMajors);
        QVERIFY(reply.value(QStringLiteral("features")).isObject());
        QCOMPARE(int(client.agreedMajor()), sent);
        return;
    }

    QTRY_VERIFY(!station->isOpen());
    // Neither a hello nor the token left this client.
    QVERIFY(station->received().isEmpty());
    const MajorList named = stationMajors.isEmpty() ? MajorList{quint16(stationMajor)}
                                                    : stationMajors;
    QCOMPARE(client.lastError(), SessionEndReasons::versionRefused(named, clientMajors));
    // R-R3-38: this client's own refusal records the same version end as
    // the Core's, so the window offers the same next steps.
    const StationEndReport report = client.lastEndReport();
    QCOMPARE(report.kind, StationEndReport::Kind::VersionRefused);
    QCOMPARE(report.reason, client.lastError());
    QCOMPARE(report.coreMajor, int(*std::max_element(named.cbegin(), named.cend())));
    QCOMPARE(report.appMajor, int(*std::max_element(clientMajors.cbegin(), clientMajors.cend())));
    QVERIFY(OperatorWording::isPlain(client.lastError()));
    QVERIFY(!client.isReconnectPending());
    QCOMPARE(retries.count(), 0);
}

void TstLinkVersion::stationDeclaresWhatItsHelloDeclared()
{
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    auto* station = new LoopbackTransport(QStringLiteral("fake-station"), this);
    auto* app = new LoopbackTransport(QStringLiteral("desktop"), this);
    station->linkTo(app);
    client.startSession(app, QStringLiteral("not-a-real-token"));
    QVERIFY(!client.stationDeclares("pairing", 1));
    station->sendText(SessionMessages::encode(SessionMessages::hello(
        1, kSessionProtocolMinor, kStationSchema, QStringLiteral("fake-station"), MajorList{1},
        {{"pairing", 1}, {"somethingNotYetInvented", 4}})));
    QTRY_VERIFY(client.stationDeclares("pairing", 1));
    QVERIFY(!client.stationDeclares("pairing", 2));
    QVERIFY(!client.stationDeclares("deviceAuth", 1));

    // A new attach forgets the previous station's declarations until its
    // own hello arrives; an older station declares nothing.
    auto* older = new LoopbackTransport(QStringLiteral("older-station"), this);
    auto* app2 = new LoopbackTransport(QStringLiteral("desktop-2"), this);
    older->linkTo(app2);
    client.startSession(app2, QStringLiteral("not-a-real-token"));
    QVERIFY(!client.stationDeclares("pairing", 1));
    older->sendText(SessionMessages::encode(SessionMessages::hello(
        1, kSessionProtocolMinor, kStationSchema, QStringLiteral("older-station"))));
    QTRY_VERIFY(!lastOfType(older->received(), QStringLiteral("auth.request")).isEmpty());
    QVERIFY(!client.stationDeclares("pairing", 1));
}

// ── nereusd's --test-link-majors ─────────────────────────────────────────

void TstLinkVersion::testLinkMajorsOption()
{
    QString error;
    QCOMPARE(LinkVersion::parseMajorList(QStringLiteral("1,2"), &error), (MajorList{1, 2}));
    QVERIFY(error.isEmpty());
    QCOMPARE(LinkVersion::parseMajorList(QStringLiteral(" 3 , 2,3"), &error), (MajorList{2, 3}));
    for (const QString& bad : {QStringLiteral(""), QStringLiteral("0"), QStringLiteral("1,x"),
                               QStringLiteral("65536"), QStringLiteral("-1"),
                               QStringLiteral("1,,2")}) {
        error.clear();
        QVERIFY2(LinkVersion::parseMajorList(bad, &error).isEmpty(), qPrintable(bad));
        QVERIFY(OperatorWording::isPlain(error));
    }

    // Not given: the build's own list, in any build.
    error.clear();
    QCOMPARE(LinkVersion::resolveTestLinkMajors(false, QString(), false, &error),
             LinkVersion::supportedMajors());
    QVERIFY(error.isEmpty());
    // Given to a debug build: the list it names.
    QCOMPARE(LinkVersion::resolveTestLinkMajors(true, QStringLiteral("1,2"), true, &error),
             (MajorList{1, 2}));
    QVERIFY(error.isEmpty());
    // Given to a release build: refused, in plain words.
    QVERIFY(LinkVersion::resolveTestLinkMajors(true, QStringLiteral("1,2"), false, &error)
                .isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(OperatorWording::isPlain(error));

#ifdef QT_NO_DEBUG
    QVERIFY(!LinkVersion::testLinkMajorsAllowed());
#else
    QVERIFY(LinkVersion::testLinkMajorsAllowed());
#endif
}

// ── TLS ──────────────────────────────────────────────────────────────────

void TstLinkVersion::stationTlsMinimumIsTls12OrLater()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP("Qt reports no working TLS backend on this machine");
    }
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
    RadioModel model;
    StationServer server(&model, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));
    // Read back from the listener itself, not from the code that built it.
    QCOMPARE(server.tlsConfiguration().protocol(), QSsl::TlsV1_2OrLater);
    server.close();
}

namespace {

enum class HandshakeOutcome { Encrypted, Refused, NoAnswer };

/// Opens a client offering only `protocol` to `port` on this machine and
/// reports whether the handshake completed. The certificate is not
/// checked: the question is only which protocol versions the listener
/// agrees to.
HandshakeOutcome handshakeTo(quint16 port, QSsl::SslProtocol protocol)
{
    QSslSocket client;
    QSslConfiguration config = client.sslConfiguration();
    config.setProtocol(protocol);
    // OpenSSL 3 refuses TLS 1.0 and 1.1 below security level 0; the
    // client lowers its own level so that only the listener decides.
    config.setBackendConfigurationOption(QByteArrayLiteral("CipherString"),
                                         QByteArrayLiteral("DEFAULT@SECLEVEL=0"));
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    client.setSslConfiguration(config);
    QSignalSpy encrypted(&client, &QSslSocket::encrypted);
    QSignalSpy failed(&client, &QAbstractSocket::errorOccurred);
    QSignalSpy disconnected(&client, &QAbstractSocket::disconnected);
    client.connectToHostEncrypted(QHostAddress(QHostAddress::LocalHost).toString(), port);
    // Event-driven: returns as soon as the handshake ends either way.
    (void)QTest::qWaitFor([&]() {
        return !encrypted.isEmpty() || !failed.isEmpty() || !disconnected.isEmpty();
    }, 10000);
    if (!encrypted.isEmpty()) {
        return HandshakeOutcome::Encrypted;
    }
    return failed.isEmpty() && disconnected.isEmpty() ? HandshakeOutcome::NoAnswer
                                                      : HandshakeOutcome::Refused;
}

} // namespace

void TstLinkVersion::stationRefusesATls11Handshake()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP("Qt reports no working TLS backend on this machine");
    }
    QTemporaryDir settingsDir;
    AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
    RadioModel model;
    StationServer server(&model, settings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    // The control: a listener of this test's own, with the station's
    // certificate and key, that allows TLS 1.1 (protocol TLS 1.0 or later
    // and OpenSSL security level 0: two settings unlike the station's). If
    // a TLS 1.1 client cannot finish a handshake even there, this
    // platform's TLS library will not offer TLS 1.1 at all. The refusal
    // below shows the station's listener refuses the client; it does not
    // tell the explicit minimum from OpenSSL 3's default security level,
    // which refuses TLS 1.0 and 1.1 on its own.
    QSslServer control;
    QSslConfiguration permissive = server.tlsConfiguration();
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    permissive.setProtocol(QSsl::TlsV1_0OrLater);
    QT_WARNING_POP
    permissive.setBackendConfigurationOption(QByteArrayLiteral("CipherString"),
                                             QByteArrayLiteral("DEFAULT@SECLEVEL=0"));
    control.setSslConfiguration(permissive);
    QVERIFY(control.listen(QHostAddress::LocalHost, 0));
    // QSsl::TlsV1_1 is deprecated in Qt 6; it is exactly what this test
    // offers.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    constexpr QSsl::SslProtocol kTls11 = QSsl::TlsV1_1;
    QT_WARNING_POP
    // The control listener itself works: a TLS 1.2 client gets through.
    QCOMPARE(handshakeTo(control.serverPort(), QSsl::TlsV1_2), HandshakeOutcome::Encrypted);
    if (handshakeTo(control.serverPort(), kTls11) != HandshakeOutcome::Encrypted) {
        QSKIP("This platform's TLS library cannot make a TLS 1.1 connection, even to a "
              "listener that allows it, so the station's refusal cannot be shown here. "
              "stationTlsMinimumIsTls12OrLater still reads the minimum back.");
    }
    control.close();

    // The station's own listener refuses the same client.
    QCOMPARE(handshakeTo(server.serverPort(), kTls11), HandshakeOutcome::Refused);
    // And serves one that offers TLS 1.2.
    QCOMPARE(handshakeTo(server.serverPort(), QSsl::TlsV1_2), HandshakeOutcome::Encrypted);
    server.close();
}

QTEST_MAIN(TstLinkVersion)
#include "tst_link_version.moc"
