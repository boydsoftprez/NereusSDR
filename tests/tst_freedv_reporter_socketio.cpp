// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: This test contains fabricated FreeDV Reporter event
// fixtures (synthetic sids and callsigns inside Engine.IO / Socket.IO
// JSON envelopes). They are test fixtures, not ported station data.
// Precedent: B2 (commit f43582e), B3 (commit 9aa4202), B4 (commit
// fce661fd).
//
// NereusSDR - FreeDVReporterClient Engine.IO / Socket.IO parser tests
//
// Phase 3J-2 Task B5. Pins the wire-protocol contract that
// FreeDVReporterClient implements:
//   - Engine.IO Open ("0{...}") parses pingInterval and prompts the
//     client to send Socket.IO Connect with the view-only auth payload
//     `{"role":"view","protocol_version":2}`.
//   - Engine.IO Ping ("2") triggers a Pong ("3") reply.
//   - Socket.IO Connect ACK ("0...") marks the client connected.
//   - Socket.IO Event ("2[...]") `new_connection` updates the station
//     map and emits `stationAdded(sid, info)`.
//   - Socket.IO Event ("2[...]") `freq_change` updates the station map,
//     emits `stationUpdated(sid, info)`, AND (NereusSDR dual-feed)
//     emits `spotReceived(DxSpot)` synthesized from station state.
//   - Socket.IO Event ("2[...]") `rx_report` emits `spotReceived(DxSpot)`
//     for the reported transmitter (its sid, picked up from the
//     receiver's freq state).
//   - Socket.IO Event ("2[...]") `remove_connection` emits
//     `stationRemoved(sid)`.

#include <QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "core/FreeDVReporterClient.h"
#include "core/FreeDVStation.h"
#include "core/DxSpot.h"

using namespace NereusSDR;

// Wrap a Socket.IO Event in the on-the-wire form: Engine.IO type '4'
// (Message) + Socket.IO type '2' (Event) + JSON array
// `["event_name", {...}]`.
static QString makeSocketIoEvent(const QString& name, const QJsonObject& data) {
    QJsonArray arr;
    arr.append(name);
    arr.append(data);
    return QStringLiteral("42")
           + QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

class TestFreeDVReporterSocketIo : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void engineIOOpenParsesPingIntervalAndSendsAuth();
    void engineIOPingTriggersPongReply();
    void socketIOConnectAckMarksConnected();
    void newConnectionEmitsStationAdded();
    void freqChangeEmitsStationUpdatedAndSpot();
    void rxReportEmitsSpotForTransmitter();
    void removeConnectionEmitsStationRemoved();
};

void TestFreeDVReporterSocketIo::initTestCase() {
    // qRegisterMetaType() is normally needed for QSignalSpy. Q_DECLARE_
    // METATYPE on DxSpot + FreeDVStation makes this automatic at first
    // use, but the explicit call documents the dependency.
    qRegisterMetaType<NereusSDR::DxSpot>("NereusSDR::DxSpot");
    qRegisterMetaType<NereusSDR::FreeDVStation>("NereusSDR::FreeDVStation");
}

void TestFreeDVReporterSocketIo::engineIOOpenParsesPingIntervalAndSendsAuth() {
    FreeDVReporterClient c;

    // Engine.IO Open: type '0' followed by JSON object with sid +
    // pingInterval + pingTimeout. Freedv-gui's qso.freedv.org server
    // sends pingInterval=25000 in production; we use 30000 here to
    // verify the value gets parsed (not a hard-coded default).
    QJsonObject openObj;
    openObj["sid"] = QStringLiteral("abc123");
    openObj["pingInterval"] = 30000;
    openObj["pingTimeout"] = 20000;
    QString openMsg = QStringLiteral("0")
        + QString::fromUtf8(QJsonDocument(openObj).toJson(QJsonDocument::Compact));

    c.handleEngineIOForTest(openMsg);

    QCOMPARE(c.pingIntervalMsForTest(), 30000);

    // The client must have responded with a Socket.IO Connect packet
    // ("40" prefix) carrying the view-only auth dict.
    QString sent = c.lastSentMessageForTest();
    QVERIFY2(sent.startsWith(QStringLiteral("40")),
             qPrintable(QStringLiteral("expected '40...' Socket.IO Connect, got: ") + sent));
    QString payload = sent.mid(2);
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    QVERIFY(doc.isObject());
    QJsonObject auth = doc.object();
    QCOMPARE(auth.value("role").toString(), QStringLiteral("view"));
    QCOMPARE(auth.value("protocol_version").toInt(), 2);
}

void TestFreeDVReporterSocketIo::engineIOPingTriggersPongReply() {
    FreeDVReporterClient c;

    // Drive an Engine.IO Open first so the client is in the steady-state
    // (otherwise lastSentMessageForTest holds the prior auth string).
    QJsonObject openObj;
    openObj["sid"] = QStringLiteral("abc123");
    openObj["pingInterval"] = 25000;
    c.handleEngineIOForTest(QStringLiteral("0")
        + QString::fromUtf8(QJsonDocument(openObj).toJson(QJsonDocument::Compact)));

    c.handleEngineIOForTest(QStringLiteral("2"));  // Engine.IO Ping
    QCOMPARE(c.lastSentMessageForTest(), QStringLiteral("3"));  // Pong
}

void TestFreeDVReporterSocketIo::socketIOConnectAckMarksConnected() {
    FreeDVReporterClient c;
    QVERIFY(!c.isConnected());

    // Socket.IO Connect ACK: type '0' inside an Engine.IO Message.
    // Server sends "40{\"sid\":\"xyz\"}" on successful handshake.
    c.handleSocketIOForTest(QStringLiteral("0{\"sid\":\"xyz\"}"));
    QVERIFY(c.isConnected());
}

void TestFreeDVReporterSocketIo::newConnectionEmitsStationAdded() {
    FreeDVReporterClient c;
    QSignalSpy addSpy(&c, &FreeDVReporterClient::stationAdded);

    QJsonObject obj;
    obj["sid"] = QStringLiteral("sid-aaa");
    obj["callsign"] = QStringLiteral("VK5DGR");
    obj["grid_square"] = QStringLiteral("PF95");
    obj["version"] = QStringLiteral("freedv-gui 1.9.10");
    obj["rx_only"] = false;
    obj["connect_time"] = QStringLiteral("2026-05-10T14:00:00Z");
    obj["last_update"] = QStringLiteral("2026-05-10T14:00:00Z");

    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("new_connection"), obj).mid(1));

    QCOMPARE(addSpy.count(), 1);
    auto args = addSpy.first();
    QCOMPARE(args[0].toString(), QStringLiteral("sid-aaa"));
    auto info = args[1].value<FreeDVStation>();
    QCOMPARE(info.callsign, QStringLiteral("VK5DGR"));
    QCOMPARE(info.gridSquare, QStringLiteral("PF95"));
    QCOMPARE(info.version, QStringLiteral("freedv-gui 1.9.10"));
    QCOMPARE(info.rxOnly, false);

    QCOMPARE(c.stations().size(), 1);
    QVERIFY(c.stations().contains(QStringLiteral("sid-aaa")));
}

void TestFreeDVReporterSocketIo::freqChangeEmitsStationUpdatedAndSpot() {
    FreeDVReporterClient c;

    // First: a new_connection so we have a sid we can update.
    QJsonObject newConn;
    newConn["sid"] = QStringLiteral("sid-bbb");
    newConn["callsign"] = QStringLiteral("K6AQ");
    newConn["grid_square"] = QStringLiteral("CM87");
    newConn["version"] = QStringLiteral("freedv-gui 1.9.10");
    newConn["rx_only"] = false;
    newConn["connect_time"] = QStringLiteral("2026-05-10T14:00:00Z");
    newConn["last_update"] = QStringLiteral("2026-05-10T14:00:00Z");
    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("new_connection"), newConn).mid(1));

    // Now spy on the dual-feed signals.
    QSignalSpy updateSpy(&c, &FreeDVReporterClient::stationUpdated);
    QSignalSpy spotSpy(&c, &FreeDVReporterClient::spotReceived);

    QJsonObject freq;
    freq["sid"] = QStringLiteral("sid-bbb");
    freq["callsign"] = QStringLiteral("K6AQ");
    freq["grid_square"] = QStringLiteral("CM87");
    freq["freq"] = qint64(14236000);  // 14.236 MHz, FreeDV calling
    freq["last_update"] = QStringLiteral("2026-05-10T14:01:00Z");
    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("freq_change"), freq).mid(1));

    // stationUpdated: rich-state feed for FreeDVStationModel
    QCOMPARE(updateSpy.count(), 1);
    auto upArgs = updateSpy.first();
    QCOMPARE(upArgs[0].toString(), QStringLiteral("sid-bbb"));
    auto info = upArgs[1].value<FreeDVStation>();
    QCOMPARE(info.frequencyHz, quint64(14236000));
    QCOMPARE(info.callsign, QStringLiteral("K6AQ"));

    // spotReceived: panadapter-overlay feed (NereusSDR dual-feed)
    QCOMPARE(spotSpy.count(), 1);
    auto spot = spotSpy.first().first().value<DxSpot>();
    QCOMPARE(spot.dxCall, QStringLiteral("K6AQ"));
    QCOMPARE(spot.spotterCall, QStringLiteral("K6AQ"));  // self-reported
    QCOMPARE(spot.freqMhz, 14.236);
    QCOMPARE(spot.source, QStringLiteral("FreeDV"));
}

void TestFreeDVReporterSocketIo::rxReportEmitsSpotForTransmitter() {
    FreeDVReporterClient c;

    // Seed: receiver station "sid-rx" with frequency 14236000 set via
    // new_connection + freq_change. (No rx_report-only station has a
    // known frequency, so a spot cannot be synthesized without it.)
    QJsonObject newConn;
    newConn["sid"] = QStringLiteral("sid-rx");
    newConn["callsign"] = QStringLiteral("N6DRC");
    newConn["grid_square"] = QStringLiteral("CM87");
    newConn["version"] = QStringLiteral("freedv-gui 1.9.10");
    newConn["rx_only"] = true;
    newConn["connect_time"] = QStringLiteral("2026-05-10T14:00:00Z");
    newConn["last_update"] = QStringLiteral("2026-05-10T14:00:00Z");
    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("new_connection"), newConn).mid(1));

    QJsonObject freq;
    freq["sid"] = QStringLiteral("sid-rx");
    freq["callsign"] = QStringLiteral("N6DRC");
    freq["grid_square"] = QStringLiteral("CM87");
    freq["freq"] = qint64(14236000);
    freq["last_update"] = QStringLiteral("2026-05-10T14:01:00Z");
    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("freq_change"), freq).mid(1));

    // Spy AFTER seeding so we count only the rx_report's emission.
    QSignalSpy spotSpy(&c, &FreeDVReporterClient::spotReceived);

    QJsonObject rxRep;
    rxRep["sid"] = QStringLiteral("sid-rx");
    rxRep["receiver_callsign"] = QStringLiteral("N6DRC");
    rxRep["receiver_grid_square"] = QStringLiteral("CM87");
    rxRep["callsign"] = QStringLiteral("VK5DGR");  // the transmitter
    rxRep["snr"] = 7;
    rxRep["mode"] = QStringLiteral("700D");
    rxRep["last_update"] = QStringLiteral("2026-05-10T14:02:00Z");
    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("rx_report"), rxRep).mid(1));

    QCOMPARE(spotSpy.count(), 1);
    auto spot = spotSpy.first().first().value<DxSpot>();
    QCOMPARE(spot.dxCall, QStringLiteral("VK5DGR"));   // tx callsign
    QCOMPARE(spot.spotterCall, QStringLiteral("N6DRC"));  // rx callsign
    QCOMPARE(spot.freqMhz, 14.236);
    QCOMPARE(spot.snr, 7);
    QCOMPARE(spot.source, QStringLiteral("FreeDV"));
}

void TestFreeDVReporterSocketIo::removeConnectionEmitsStationRemoved() {
    FreeDVReporterClient c;

    QJsonObject newConn;
    newConn["sid"] = QStringLiteral("sid-ccc");
    newConn["callsign"] = QStringLiteral("VK5DGR");
    newConn["grid_square"] = QStringLiteral("PF95");
    newConn["version"] = QStringLiteral("freedv-gui 1.9.10");
    newConn["rx_only"] = false;
    newConn["connect_time"] = QStringLiteral("2026-05-10T14:00:00Z");
    newConn["last_update"] = QStringLiteral("2026-05-10T14:00:00Z");
    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("new_connection"), newConn).mid(1));
    QCOMPARE(c.stations().size(), 1);

    QSignalSpy remSpy(&c, &FreeDVReporterClient::stationRemoved);

    QJsonObject rem;
    rem["sid"] = QStringLiteral("sid-ccc");
    rem["callsign"] = QStringLiteral("VK5DGR");
    rem["grid_square"] = QStringLiteral("PF95");
    rem["version"] = QStringLiteral("freedv-gui 1.9.10");
    rem["rx_only"] = false;
    rem["last_update"] = QStringLiteral("2026-05-10T14:03:00Z");
    c.handleSocketIOForTest(makeSocketIoEvent(QStringLiteral("remove_connection"), rem).mid(1));

    QCOMPARE(remSpy.count(), 1);
    QCOMPARE(remSpy.first().first().toString(), QStringLiteral("sid-ccc"));
    QCOMPARE(c.stations().size(), 0);
}

QTEST_MAIN(TestFreeDVReporterSocketIo)
#include "tst_freedv_reporter_socketio.moc"
