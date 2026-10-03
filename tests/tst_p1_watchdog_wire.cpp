// no-port-check: test-only -- HL2 firmware file names and Thetis source
// paths appear only in source-cite comments that document which upstream line
// each assertion verifies.  No Thetis or deskhpsdr logic is ported here;
// this file is NereusSDR-original.
//
// Wire-byte snapshot tests for the RUNSTOP packet byte 3 (3M-1a Task E.5,
// rewritten for R-R3-49).
//
// R-R3-49: the Network Watchdog setting is NOT sent on Protocol 1. Thetis
// and mi0bot-Thetis send the same start and stop commands whatever the
// setting; on P1 the setting only sets how long the read loop waits for data
// (tst_network_watchdog covers that).
//   Thetis networkproto1.c:50 [v2.10.3.15]:  outpacket.packetbuf[3] = 0x01;
//   Thetis networkproto1.c:85 [v2.10.3.15]:  outpacket.packetbuf[3] = 0x00;
//   mi0bot-Thetis networkproto1.c:50, 85 [@c26a8a4]: the same.
//
// Bit 7 of byte 3 is the HL2 gateware's watchdog_disable
// (Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400). NereusSDR used to set
// it when the watchdog was off; it now stays 0, so the HL2's own watchdog
// stays on as it does under Thetis.
//
// RUNSTOP packet layout (64 bytes):
//   pkt[0] = 0xEF, pkt[1] = 0xFE, pkt[2] = 0x04
//   pkt[3] = 0x01 start IQ only, 0x02 start IQ + mic, 0x00 stop
//   pkt[4..63] = 0x00 (padding)
//
// The checks read the datagrams a loopback P1FakeRadio receives from a
// connected P1RadioConnection, not a copy of the packet composed for the
// test, so they see what actually goes on the wire.
//
// Modification history (NereusSDR):
//   2026-09-24: R-R3-49, the setting no longer changes byte 3 (Thetis
//               parity), by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-24: R-R3-49 fix wave: assert on the datagrams received by the
//               loopback fake, by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
#include <QtTest/QtTest>

#include <QElapsedTimer>

#include <memory>

#include "core/HpsdrModel.h"
#include "core/P1RadioConnection.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

namespace {

RadioInfo p1Info(const P1FakeRadio& fake)
{
    RadioInfo info;
    info.address = fake.localAddress();
    info.port = fake.localPort();
    info.boardType = HPSDRHW::HermesLite;
    info.protocol = ProtocolVersion::Protocol1;
    info.firmwareVersion = 72;
    info.macAddress = QStringLiteral("aa:bb:cc:49:00:02");
    return info;
}

// Bring a P1 link up against the fake, retrying if the connect deadline
// tears an attempt down on a loaded machine (as tst_network_watchdog does).
std::unique_ptr<P1RadioConnection> bringUp(const P1FakeRadio& fake, bool watchdogOn)
{
    for (int attempt = 0; attempt < 4; ++attempt) {
        auto conn = std::make_unique<P1RadioConnection>();
        conn->init();
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

// The exact 64-byte RUNSTOP packet: EF FE 04 <cmd>, then zeros
// (networkproto1.c:45 memset, 47-50 [v2.10.3.15]).
QByteArray runStop(quint8 cmd)
{
    QByteArray pkt(64, '\0');
    pkt[0] = char(0xEF);
    pkt[1] = char(0xFE);
    pkt[2] = char(0x04);
    pkt[3] = char(cmd);
    return pkt;
}

// Connect with the setting given, change it while connected, disconnect,
// and return every start/stop datagram the fake received (none when the
// link did not come up or no stop arrived).
QList<QByteArray> runStopDatagrams(bool watchdogOn)
{
    P1FakeRadio fake;
    fake.start();
    std::unique_ptr<P1RadioConnection> conn = bringUp(fake, watchdogOn);
    if (!conn || !QTest::qWaitFor([&fake] { return fake.isRunning(); }, 2000)) {
        return {};
    }
    conn->setWatchdogEnabled(!watchdogOn);
    conn->disconnect();
    if (!QTest::qWaitFor([&fake] { return fake.metisStopCount() >= 1; }, 2000)) {
        return {};
    }
    return fake.metisCommandsReceived();
}

} // namespace

class TestP1WatchdogWire : public QObject {
    Q_OBJECT
private slots:

    // The start datagram is EF FE 04 01 and 60 zeros, and the stop datagram
    // EF FE 04 00 and 60 zeros, with the watchdog on or off and when it is
    // changed while connected (networkproto1.c:50, 85 [v2.10.3.15]). Bit 7
    // of byte 3 (the HL2 gateware's watchdog_disable) is never set.
    void runStopDatagramsIgnoreTheSetting_data()
    {
        QTest::addColumn<bool>("watchdogOn");
        QTest::newRow("on at connect") << true;
        QTest::newRow("off at connect") << false;
    }

    void runStopDatagramsIgnoreTheSetting()
    {
        QFETCH(bool, watchdogOn);
        const QList<QByteArray> received = runStopDatagrams(watchdogOn);
        QVERIFY2(!received.isEmpty(), "the link did not come up against the fake, or no stop arrived");
        const QByteArray start = runStop(0x01);
        const QByteArray stop = runStop(0x00);
        int starts = 0;
        for (const QByteArray& datagram : received) {
            QCOMPARE(datagram.size(), 64);
            QCOMPARE(int(quint8(datagram[3])) & 0x80, 0);
            QVERIFY2(datagram == start || datagram == stop,
                     qPrintable(QString::fromLatin1(datagram.toHex(' '))));
            starts += datagram == start ? 1 : 0;
        }
        QVERIFY(starts >= 1);
        QCOMPARE(received.last(), stop);
    }
};

QTEST_GUILESS_MAIN(TestP1WatchdogWire)
#include "tst_p1_watchdog_wire.moc"
