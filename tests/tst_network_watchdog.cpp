// no-port-check: NereusSDR-original test. Thetis file names and lines appear
// only in comments naming the behaviour each assertion checks.
//
// tests/tst_network_watchdog.cpp
//
// R-R3-49 / R-R3-11: the Network Watchdog setting (Setup > General >
// Options) does what Thetis does with it.
//
// Protocol 2 (loopback P2FakeRadio): the radio's own safety timer stays on
// whatever the setting says. Byte 38 of the general command packet is
// always 1 and the 500 ms keepalive general packet always runs, so a radio
// left keyed when the computer dies still drops out of transmit (operator
// decision 2026-09-24). This is a deliberate divergence: Thetis lets both
// follow the setting (ChannelMaster/network.c:897-898, 1436 [v2.10.3.15]).
// The setting governs only the wait for data: with it off no loss is
// declared when data stops (network.c:656: prn->wdt ? 3000 : WSA_INFINITE).
//
// Protocol 1 (loopback P1FakeRadio): nothing on the wire changes; the start
// and stop packets stay 0x01 / 0x00 (networkproto1.c:50 [v2.10.3.15]). The
// setting sets how long NereusSDR waits for data before it declares the
// radio lost: 3000 ms with it on, no loss declared with it off
// (networkproto1.c:294 [v2.10.3.15]).
//
// No hardware; no audio device is opened.
//
// Modification history (NereusSDR):
//   2026-09-24: created (R-R3-49, R-R3-21, R-R3-11), by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QSignalSpy>

#include <functional>
#include <memory>

#include "core/HpsdrModel.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "fakes/P1FakeRadio.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;
using NereusSDR::Test::P2FakeRadio;

namespace {

constexpr int kP2ConnectTimeoutMs = 180;
constexpr int kP2EstablishedTimeoutMs = 220;
constexpr int kP1TestSilenceMs = 150;
constexpr int kP1TestReconnectMs = 300;

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 3000)
{
    return QTest::qWaitFor(predicate, timeoutMs);
}

// Loss reports only; an HL2 link also reports a busy network as an error.
int lossReports(const QSignalSpy& errors)
{
    int count = 0;
    for (const QList<QVariant>& call : errors) {
        if (call.at(0).value<RadioConnectionError>() == RadioConnectionError::NoDataTimeout) {
            ++count;
        }
    }
    return count;
}

void configureP2(P2RadioConnection& connection, const P2FakeRadio& fake)
{
    connection.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
    connection.setSilenceTimeoutsForTest(kP2ConnectTimeoutMs, kP2EstablishedTimeoutMs);
    connection.init();
}

bool establishP2(P2RadioConnection& connection, P2FakeRadio& fake)
{
    connection.connectToRadio(fake.radioInfo());
    if (!waitUntil([&fake]() { return fake.hasClient(); })) {
        return false;
    }
    fake.sendDdc(2);
    return waitUntil([&connection]() {
        return connection.state() == ConnectionState::Connected;
    });
}

RadioInfo p1Info(const P1FakeRadio& fake)
{
    RadioInfo info;
    info.address = fake.localAddress();
    info.port = fake.localPort();
    info.boardType = HPSDRHW::HermesLite;
    info.protocol = ProtocolVersion::Protocol1;
    info.firmwareVersion = 72;
    info.macAddress = QStringLiteral("aa:bb:cc:49:00:01");
    return info;
}

// Bring a P1 link up against the fake, retrying if the connect deadline tears
// an attempt down on a loaded machine (see tst_reconnect_on_silence.cpp).
std::unique_ptr<P1RadioConnection> bringP1Up(const P1FakeRadio& fake, bool watchdogOn)
{
    for (int attempt = 0; attempt < 4; ++attempt) {
        auto conn = std::make_unique<P1RadioConnection>();
        conn->init();
        conn->setReconnectTimingForTest(kP1TestSilenceMs, kP1TestReconnectMs);
        conn->setWatchdogEnabled(watchdogOn);
        conn->connectToRadio(p1Info(fake));
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < 3000) {
            if (conn->state() == ConnectionState::Connected) {
                return conn;
            }
            if (conn->state() == ConnectionState::Disconnected) {
                break;
            }
            QTest::qWait(10);
        }
    }
    return nullptr;
}

} // namespace

class TestNetworkWatchdog final : public QObject {
    Q_OBJECT

private slots:
    // ---- Protocol 2: on the wire ------------------------------------------

    void p2WatchdogOnAtConnectSendsByte38One()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureP2(connection, fake);
        // Default on, as Thetis's checkbox (setup.designer.cs:8434).
        QVERIFY(connection.isWatchdogEnabled());
        QVERIFY(establishP2(connection, fake));
        QTRY_VERIFY(fake.generalDatagrams() >= 1);
        QCOMPARE(fake.lastGeneralWatchdog(), 1);
    }

    void p2WatchdogOffAtConnectStillSendsByte38One()
    {
        // The radio's safety timer stays on with the setting off.
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureP2(connection, fake);
        connection.setWatchdogEnabled(false);
        QVERIFY(establishP2(connection, fake));
        QTRY_VERIFY(fake.generalDatagrams() >= 1);
        QCOMPARE(fake.lastGeneralWatchdog(), 1);
    }

    void p2KeepaliveRunsWithByte38OneWhateverTheSetting()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureP2(connection, fake);
        // Established-silence loss is not under test here; keep data coming.
        QTimer feeder;
        QObject::connect(&feeder, &QTimer::timeout, &fake, [&fake]() { fake.sendDdc(2); });
        QVERIFY(establishP2(connection, fake));
        feeder.start(50);

        int before = fake.generalDatagrams();
        QTest::qWait(1300);
        QVERIFY2(fake.generalDatagrams() - before >= 2,
                 "keepalive general packets every 500 ms with the setting on");
        QCOMPARE(fake.lastGeneralWatchdog(), 1);

        connection.setWatchdogEnabled(false);
        before = fake.generalDatagrams();
        QTest::qWait(1300);
        QVERIFY2(fake.generalDatagrams() - before >= 2,
                 "keepalive general packets every 500 ms with the setting off");
        QCOMPARE(fake.lastGeneralWatchdog(), 1);
        QCOMPARE(connection.state(), ConnectionState::Connected);

        connection.setWatchdogEnabled(true);
        before = fake.generalDatagrams();
        QTest::qWait(1300);
        QVERIFY(fake.generalDatagrams() - before >= 2);
        QCOMPARE(fake.lastGeneralWatchdog(), 1);
    }

    // ---- Protocol 2: loss detection ---------------------------------------

    void p2LossIsDeclaredAfterTheWaitWithTheWatchdogOn()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureP2(connection, fake);
        QSignalSpy errors(&connection, &RadioConnection::errorOccurred);
        QVERIFY(establishP2(connection, fake));
        errors.clear();
        fake.stopIngress();
        QTRY_COMPARE_WITH_TIMEOUT(connection.state(), ConnectionState::LinkLost, 3000);
        QCOMPARE(lossReports(errors), 1);
    }

    void p2NoLossIsDeclaredWithTheWatchdogOff()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureP2(connection, fake);
        connection.setWatchdogEnabled(false);
        QSignalSpy errors(&connection, &RadioConnection::errorOccurred);
        QVERIFY(establishP2(connection, fake));
        errors.clear();
        fake.stopIngress();
        QTest::qWait(kP2EstablishedTimeoutMs * 4);
        QCOMPARE(connection.state(), ConnectionState::Connected);
        QCOMPARE(lossReports(errors), 0);
    }

    void p2TurningTheWatchdogOffStopsAWaitInProgress()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureP2(connection, fake);
        QVERIFY(establishP2(connection, fake));
        fake.stopIngress();
        connection.setWatchdogEnabled(false);
        QTest::qWait(kP2EstablishedTimeoutMs * 4);
        QCOMPARE(connection.state(), ConnectionState::Connected);
    }

    void p2TurningTheWatchdogOnStartsTheWaitFromThen()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureP2(connection, fake);
        connection.setWatchdogEnabled(false);
        QVERIFY(establishP2(connection, fake));
        fake.stopIngress();
        QTest::qWait(kP2EstablishedTimeoutMs * 2);
        QCOMPARE(connection.state(), ConnectionState::Connected);
        connection.setWatchdogEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(connection.state(), ConnectionState::LinkLost, 3000);
    }

    // ---- Protocol 1: the wait ---------------------------------------------

    void p1WaitIsThetisThreeSeconds()
    {
        P1RadioConnection connection;
        QCOMPARE(connection.watchdogSilenceMsForTest(), 3000);
        QVERIFY(connection.isWatchdogEnabled());
    }

    void p1StartAndStopDatagramsDoNotCarryTheSetting_data()
    {
        QTest::addColumn<bool>("watchdogOn");
        QTest::newRow("on") << true;
        QTest::newRow("off") << false;
    }

    void p1StartAndStopDatagramsDoNotCarryTheSetting()
    {
        // Thetis networkproto1.c:50 and 85: 0x01 to start, 0x00 to stop,
        // whatever the setting. Asserted on the datagrams the fake radio
        // received, not on a copy composed for the test.
        QFETCH(bool, watchdogOn);
        P1FakeRadio fake;
        fake.start();
        std::unique_ptr<P1RadioConnection> conn = bringP1Up(fake, watchdogOn);
        QVERIFY(conn != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 2000);
        conn->disconnect();
        QTRY_VERIFY_WITH_TIMEOUT(fake.metisStopCount() >= 1, 2000);

        QByteArray start(64, '\0');
        start[0] = char(0xEF);
        start[1] = char(0xFE);
        start[2] = char(0x04);
        QByteArray stop = start;
        start[3] = char(0x01);
        stop[3] = char(0x00);
        const QList<QByteArray>& received = fake.metisCommandsReceived();
        QVERIFY(!received.isEmpty());
        int starts = 0;
        for (const QByteArray& datagram : received) {
            QVERIFY(datagram == start || datagram == stop);
            starts += datagram == start ? 1 : 0;
        }
        QVERIFY(starts >= 1);
        QCOMPARE(received.last(), stop);
    }

    void p1LossIsDeclaredAfterTheWaitWithTheWatchdogOn()
    {
        P1FakeRadio fake;
        fake.start();
        std::unique_ptr<P1RadioConnection> conn = bringP1Up(fake, true);
        QVERIFY(conn != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 2000);
        QSignalSpy errors(conn.get(), &RadioConnection::errorOccurred);
        fake.goSilent();
        QTRY_COMPARE_WITH_TIMEOUT(conn->state(), ConnectionState::LinkLost, 3000);
        QCOMPARE(lossReports(errors), 1);
    }

    void p1NoLossIsDeclaredWithTheWatchdogOff()
    {
        P1FakeRadio fake;
        fake.start();
        std::unique_ptr<P1RadioConnection> conn = bringP1Up(fake, false);
        QVERIFY(conn != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 2000);
        QSignalSpy errors(conn.get(), &RadioConnection::errorOccurred);
        fake.goSilent();
        QTest::qWait(kP1TestSilenceMs * 5);
        QCOMPARE(conn->state(), ConnectionState::Connected);
        QCOMPARE(lossReports(errors), 0);
    }

    void p1TurningTheWatchdogOnStartsTheWaitFromThen()
    {
        P1FakeRadio fake;
        fake.start();
        std::unique_ptr<P1RadioConnection> conn = bringP1Up(fake, false);
        QVERIFY(conn != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 2000);
        fake.goSilent();
        QTest::qWait(kP1TestSilenceMs * 3);
        QCOMPARE(conn->state(), ConnectionState::Connected);
        conn->setWatchdogEnabled(true);
        // Not at once: the wait starts when the watchdog is turned on.
        QTest::qWait(kP1TestSilenceMs / 3);
        QCOMPARE(conn->state(), ConnectionState::Connected);
        QTRY_COMPARE_WITH_TIMEOUT(conn->state(), ConnectionState::LinkLost, 3000);
    }
};

QTEST_MAIN(TestNetworkWatchdog)
#include "tst_network_watchdog.moc"
