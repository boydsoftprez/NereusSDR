// no-port-check: NereusSDR-original test. Thetis file names appear only as
// cites for the behavior asserted.
// =================================================================
// tests/tst_p2_cw_wire.cpp  (NereusSDR)
// =================================================================
//
// The Protocol 2 CW wire (R-CW-16), through the production UDP path to a
// loopback fake radio (P2FakeRadio), nothing keyed:
//   - setCwWireState with a change sends CmdTx once, with byte 5
//     composed (composeP2CwModeControl) and edge length 9 in byte 17; the
//     same state again sends nothing (Thetis netInterface.c SetCW*);
//   - a key change from another thread sends high priority on the
//     connection thread with byte 5 = dash<<2 | dot<<1 | cwx; a burst of
//     changes made before dispatch coalesces to one send carrying the
//     last state (Thetis netInterface.c SetCWDot, network.c:928-933);
//   - with qskTxAttenuation, CmdTx bytes 57-59 are 31;
//   - under the transmit cut, CmdTx byte 5 bits 1 and 7 and high-priority
//     byte 5 read 0 synchronously, and both packets reach the fake.
// Logical ordering uses queued-call and packet barriers; no latency is measured.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: Final fix wave F3: packet and queued-call barriers replace
//               wall-clock settling in logical CW wire checks. J.J. Boyd
//               (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26: CW transmit, the P2 CW wire tests. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
//   2026-10-26: CW transmit early fix wave I1: clearing the cut sends
//               CmdTx and high priority. J.J. Boyd (KG4VCF), AI-assisted
//               via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QTimer>

#include <memory>
#include <thread>

#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P2FakeRadio;

namespace {

// Only a failure bound for receiving an expected packet, not a latency claim.
constexpr int kPacketWaitMs = 2000;

void configureConnection(P2RadioConnection& connection, const P2FakeRadio& fake)
{
    connection.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
    connection.init();
}

// As tst_p2_established_silence: connects, event driven, bounded by 3 s.
bool establish(P2RadioConnection& connection, P2FakeRadio& fake)
{
    QEventLoop loop;
    bool connected = false;
    const QMetaObject::Connection onState = QObject::connect(
        &connection, &RadioConnection::connectionStateChanged, &loop,
        [&loop, &connected](ConnectionState state) {
            if (state == ConnectionState::Connected) {
                connected = true;
                loop.quit();
            }
        });
    const QMetaObject::Connection onFailed = QObject::connect(
        &connection, &RadioConnection::connectFailed, &loop, &QEventLoop::quit);
    bool sent = false;
    QTimer firstDdc;
    firstDdc.setInterval(0);
    QObject::connect(&firstDdc, &QTimer::timeout, &loop, [&fake, &sent, &firstDdc]() {
        if (!sent && fake.hasClient()) {
            sent = true;
            firstDdc.stop();
            fake.sendDdc(2);
        }
    });
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);

    connection.connectToRadio(fake.radioInfo());
    if (connection.state() != ConnectionState::Connected) {
        firstDdc.start();
        limit.start(3000);
        loop.exec();
    }
    QObject::disconnect(onState);
    QObject::disconnect(onFailed);
    return connected || connection.state() == ConnectionState::Connected;
}

// Each fake readyRead handler drains its role socket. A distinguishable
// packet on the same role is the receive barrier for earlier sends; counts
// after it include duplicate packets before that barrier. RX has no periodic
// CmdTx/high-priority sends (P2RadioConnection's heartbeat is gated on MOX).
int lastByte(const QVector<QByteArray>& packets, int offset)
{
    return packets.isEmpty() ? -1 : int(quint8(packets.last().at(offset)));
}

// Writers have completed before this call. Deliver their queued key update
// explicitly, without allowing timer events to decide how bursts coalesce.
void flushKeyChanges(P2RadioConnection& connection)
{
    QCoreApplication::sendPostedEvents(&connection, QEvent::MetaCall);
}

CwWireState keyerOn()
{
    CwWireState s;
    s.enable = true;
    s.breakIn = true;
    s.speedWpm = 30;
    s.sidetoneLevel = 40;
    return s;
}

} // namespace

class TestP2CwWire : public QObject {
    Q_OBJECT
private slots:
    void setCwWireStateSendsCmdTxOnce()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        QVERIFY(connection.p2Codec() != nullptr);
        // A different speed marks the end of startup traffic on CmdTx.
        CwWireState baseline = keyerOn();
        baseline.speedWpm = 29;
        connection.setCwWireState(baseline);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.txPackets(), 9), 29, kPacketWaitMs);

        const auto txCount = [&fake]() { return int(fake.txPackets().size()); };
        const int before = txCount();
        const CwWireState s = keyerOn();
        connection.setCwWireState(s);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.txPackets(), 9), 30, kPacketWaitMs);
        QCOMPARE(txCount(), before + 1);

        const QByteArray pkt = fake.txPackets().last();
        CwWireState clamped = s;
        applyCwWireClamps(clamped);
        QCOMPARE(int(quint8(pkt[5])), int(composeP2CwModeControl(clamped, false)));
        QCOMPARE(int(quint8(pkt[5])) & 0x82, 0x82);   // enable, break-in
        QCOMPARE(int(quint8(pkt[6])), 40);            // sidetone level
        QCOMPARE(int(quint8(pkt[9])), 30);            // speed
        QCOMPARE(int(quint8(pkt[17])), 9);            // edge length

        // The same state again: nothing. A later changed state is a packet
        // barrier, so this negative assertion needs no settling interval.
        connection.setCwWireState(s);
        flushKeyChanges(connection);
        CwWireState marker = s;
        marker.speedWpm = 31;
        connection.setCwWireState(marker);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.txPackets(), 9), 31, kPacketWaitMs);
        QCOMPARE(txCount(), before + 2);
        QCOMPARE(int(quint8(fake.txPackets().at(before)[9])), 30);
    }

    void keyBurstFromAnotherThreadSendsHighPriorityBounded()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        connection.setCwWireState(keyerOn());
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.txPackets(), 9), 30, kPacketWaitMs);

        const std::shared_ptr<CwKeyLine> line = connection.cwKeyLine();
        QVERIFY(line);
        // Distinguishable key packets drain startup traffic before counting.
        line->setDash(true);
        flushKeyChanges(connection);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.highPriorityPackets(), 5), 0x04, kPacketWaitMs);
        line->setDash(false);
        flushKeyChanges(connection);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.highPriorityPackets(), 5), 0x00, kPacketWaitMs);
        const auto hpCount = [&fake]() { return int(fake.highPriorityPackets().size()); };
        const int before = hpCount();

        // 50 changes, then a final state: dash and CWX down, dot up.
        std::thread writer([line]() {
            for (int i = 0; i < 50; ++i) {
                line->setDot((i & 1) == 0);
            }
            line->setDash(true);
            line->setCwx(true);
        });
        writer.join();

        // No event dispatch occurred while the writer ran, so the whole
        // burst is pending now and must coalesce to exactly one packet.
        QCOMPARE(hpCount(), before);
        flushKeyChanges(connection);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.highPriorityPackets(), 5), 0x05, kPacketWaitMs);
        QCOMPARE(hpCount(), before + 1);
        const QByteArray last = fake.highPriorityPackets().last();
        // dash<<2 | dot<<1 | cwx = 4 | 0 | 1.
        QCOMPARE(int(quint8(last[5])), 0x05);

        // One more change on this thread: one more send, dot now in bit 1.
        const int before2 = hpCount();
        line->setDot(true);
        flushKeyChanges(connection);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.highPriorityPackets(), 5), 0x07, kPacketWaitMs);
        QCOMPARE(hpCount(), before2 + 1);

        // Repeating levels posts nothing. The following key-up is a second
        // distinguishable packet barrier and preserves the exact edge count.
        line->setDot(true);
        line->setDash(true);
        line->setCwx(true);
        flushKeyChanges(connection);
        line->setDot(false);
        line->setDash(false);
        line->setCwx(false);
        flushKeyChanges(connection);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.highPriorityPackets(), 5), 0x00, kPacketWaitMs);
        QCOMPARE(hpCount(), before2 + 2);
    }

    void qskTxAttenuationSends31()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        connection.setTxStepAttenuation(10);
        CwWireState s = keyerOn();
        connection.setCwWireState(s);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.txPackets(), 9), 30, kPacketWaitMs);
        QByteArray pkt = fake.txPackets().last();
        QCOMPARE(int(quint8(pkt[57])), 10);
        QCOMPARE(int(quint8(pkt[58])), 10);
        QCOMPARE(int(quint8(pkt[59])), 10);

        const auto txCount = [&fake]() { return int(fake.txPackets().size()); };
        int before = txCount();
        s.qskTxAttenuation = true;
        connection.setCwWireState(s);
        QTRY_VERIFY_WITH_TIMEOUT(txCount() > before, kPacketWaitMs);
        pkt = fake.txPackets().last();
        QCOMPARE(int(quint8(pkt[57])), 31);
        QCOMPARE(int(quint8(pkt[58])), 31);
        QCOMPARE(int(quint8(pkt[59])), 31);

        before = txCount();
        s.qskTxAttenuation = false;
        connection.setCwWireState(s);
        QTRY_VERIFY_WITH_TIMEOUT(txCount() > before, kPacketWaitMs);
        pkt = fake.txPackets().last();
        QCOMPARE(int(quint8(pkt[59])), 10);
    }

    // The safety rule: the cut zeroes CW enable, break-in and the key bits
    // synchronously in compose, then both role packets carry the cut state.
    void transmitCutZeroesCwAndSendsBoth()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        connection.setCwWireState(keyerOn());
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.txPackets(), 9), 30, kPacketWaitMs);
        connection.cwKeyLine()->setDot(true);
        connection.cwKeyLine()->setCwx(true);
        flushKeyChanges(connection);
        QTRY_COMPARE_WITH_TIMEOUT(lastByte(fake.highPriorityPackets(), 5), 0x03, kPacketWaitMs);

        const auto txCount = [&fake]() { return int(fake.txPackets().size()); };
        const auto hpCount = [&fake]() { return int(fake.highPriorityPackets().size()); };
        const int txBefore = txCount();
        const int hpBefore = hpCount();

        // The cut's own log line, expected.
        QTest::ignoreMessage(QtWarningMsg, "Transmit cut: MOX and the T/R relay off");
        connection.cutTransmit();
        quint8 tx[60] = {};
        connection.composeCmdTxForTest(tx);
        QCOMPARE(int(tx[5] & 0x82), 0);
        QVERIFY((tx[5] & 0x08) != 0);   // the rest of byte 5 is kept
        quint8 hp[1444] = {};           // P2RadioConnection::kBufLen
        connection.composeCmdHighPriorityForTest(hp);
        QCOMPARE(int(hp[5]), 0);

        QTRY_VERIFY_WITH_TIMEOUT(txCount() > txBefore, kPacketWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(hpCount() > hpBefore, kPacketWaitMs);
        for (int i = txBefore; i < txCount(); ++i) {
            QCOMPARE(int(quint8(fake.txPackets().at(i)[5])) & 0x82, 0);
        }
        for (int i = hpBefore; i < hpCount(); ++i) {
            QCOMPARE(int(quint8(fake.highPriorityPackets().at(i)[5])), 0);
            QCOMPARE(int(quint8(fake.highPriorityPackets().at(i)[4])) & 0x02, 0);
        }

        // Early fix wave I1: clearing the cut sends CmdTx and high priority
        // at once with the restored bits; nothing else would (the wire state
        // is unchanged and no heartbeat resends CmdTx while receiving).
        const int txBeforeClear = txCount();
        const int hpBeforeClear = hpCount();
        connection.clearTransmitCut();
        connection.composeCmdTxForTest(tx);
        QCOMPARE(int(tx[5] & 0x82), 0x82);
        connection.composeCmdHighPriorityForTest(hp);
        QCOMPARE(int(hp[5]), 0x03);
        QTRY_VERIFY_WITH_TIMEOUT(txCount() > txBeforeClear, kPacketWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(hpCount() > hpBeforeClear, kPacketWaitMs);
        QCOMPARE(int(quint8(fake.txPackets().last()[5])) & 0x82, 0x82);
        QCOMPARE(int(quint8(fake.highPriorityPackets().last()[5])), 0x03);
    }

    // R-CW-14: the legacy compose sends no CW. A state set before the
    // codec is chosen stays off the legacy CmdTx and high priority (the
    // compose falls back to legacy while no codec is chosen), and reaches
    // the codec compose once the codec is chosen.
    void stateBeforeCodecChoiceStaysOffLegacyCompose()
    {
        P2RadioConnection conn;
        conn.setCwWireState(keyerOn());
        conn.cwKeyLine()->setDot(true);
        flushKeyChanges(conn);   // the key line's queued call
        quint8 tx[60] = {};
        quint8 hp[1444] = {};
        conn.composeCmdTxForTest(tx);
        conn.composeCmdHighPriorityForTest(hp);
        QCOMPARE(int(tx[5]), 0);
        QCOMPARE(int(tx[9]), 0);
        QCOMPARE(int(hp[5]), 0);

        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.composeCmdTxForTest(tx);
        conn.composeCmdHighPriorityForTest(hp);
        QCOMPARE(int(tx[5]) & 0x82, 0x82);
        QCOMPARE(int(tx[9]), 30);
        QCOMPARE(int(hp[5]), 0x02);   // dot in bit 1
    }

    void legacyFlagKeepsCwOffTheWire()
    {
        qputenv("NEREUS_USE_LEGACY_P2_CODEC", "1");
        P2RadioConnection conn;
        conn.setCwWireState(keyerOn());
        conn.setBoardForTest(HPSDRHW::OrionMKII);   // selects the legacy path
        conn.cwKeyLine()->setDash(true);
        flushKeyChanges(conn);
        conn.setCwWireState(keyerOn());             // unchanged: no-op
        CwWireState faster = keyerOn();
        faster.speedWpm = 40;
        conn.setCwWireState(faster);
        quint8 tx[60] = {};
        quint8 hp[1444] = {};
        conn.composeCmdTxForTest(tx);
        conn.composeCmdHighPriorityForTest(hp);
        qunsetenv("NEREUS_USE_LEGACY_P2_CODEC");
        QCOMPARE(int(tx[5]), 0);
        QCOMPARE(int(tx[9]), 0);
        QCOMPARE(int(tx[17]), 9);
        QCOMPARE(int(hp[5]), 0);
    }
};

QTEST_GUILESS_MAIN(TestP2CwWire)
#include "tst_p2_cw_wire.moc"
