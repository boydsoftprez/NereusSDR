// no-port-check: NereusSDR-original test. Thetis file names appear only as
// cites for the behavior asserted.
// =================================================================
// tests/tst_p2_cw_wire.cpp  (NereusSDR)
// =================================================================
//
// The Protocol 2 CW wire (R-CW-16), through the production UDP path to a
// loopback fake radio (P2FakeRadio), nothing keyed:
//   - setCwWireState with a change sends CmdTx once, at once, with byte 5
//     composed (composeP2CwModeControl) and edge length 9 in byte 17; the
//     same state again sends nothing (Thetis netInterface.c SetCW*);
//   - a key change from another thread sends high priority on the
//     connection thread with byte 5 = dash<<2 | dot<<1 | cwx; a burst of
//     changes posts a bounded number of sends and the last carries the
//     last state (Thetis netInterface.c SetCWDot, network.c:928-933);
//   - with qskTxAttenuation, CmdTx bytes 57-59 are 31;
//   - under the transmit cut, CmdTx byte 5 bits 1 and 7 and high-priority
//     byte 5 read 0 at once, and both packets go out at once.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the P2 CW wire tests. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>

#include <functional>
#include <memory>
#include <thread>

#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P2FakeRadio;

namespace {

constexpr int kAtOnceMs = 500;     // "at once": well inside one status period
constexpr int kSettleMs = 150;     // long enough for a second packet to show

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

// Waits up to timeoutMs for count() to exceed from; the elapsed ms, or -1.
qint64 waitForMore(const std::function<int()>& count, int from, int timeoutMs = 2000)
{
    QElapsedTimer t;
    t.start();
    while (count() <= from && t.elapsed() < timeoutMs) {
        QTest::qWait(1);
    }
    return count() > from ? t.elapsed() : -1;
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
    void setCwWireStateSendsCmdTxOnceAtOnce()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        QVERIFY(connection.p2Codec() != nullptr);
        QTest::qWait(kSettleMs);

        const auto txCount = [&fake]() { return int(fake.txPackets().size()); };
        const int before = txCount();
        const CwWireState s = keyerOn();
        connection.setCwWireState(s);
        const qint64 ms = waitForMore(txCount, before);
        QVERIFY2(ms >= 0, "no CmdTx after setCwWireState");
        QVERIFY2(ms < kAtOnceMs, "CmdTx did not go out at once");
        QTest::qWait(kSettleMs);
        QCOMPARE(txCount(), before + 1);

        const QByteArray pkt = fake.txPackets().last();
        CwWireState clamped = s;
        applyCwWireClamps(clamped);
        QCOMPARE(int(quint8(pkt[5])), int(composeP2CwModeControl(clamped, false)));
        QCOMPARE(int(quint8(pkt[5])) & 0x82, 0x82);   // enable, break-in
        QCOMPARE(int(quint8(pkt[6])), 40);            // sidetone level
        QCOMPARE(int(quint8(pkt[9])), 30);            // speed
        QCOMPARE(int(quint8(pkt[17])), 9);            // edge length

        // The same state again: nothing.
        connection.setCwWireState(s);
        QTest::qWait(kSettleMs);
        QCOMPARE(txCount(), before + 1);
    }

    void keyBurstFromAnotherThreadSendsHighPriorityBounded()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        connection.setCwWireState(keyerOn());
        QTest::qWait(kSettleMs);

        const auto hpCount = [&fake]() { return int(fake.highPriorityPackets().size()); };
        const int before = hpCount();
        const std::shared_ptr<CwKeyLine> line = connection.cwKeyLine();
        QVERIFY(line);

        // 50 changes, then a final state: dash and CWX down, dot up.
        std::thread writer([line]() {
            for (int i = 0; i < 50; ++i) {
                line->setDot((i & 1) == 0);
            }
            line->setDash(true);
            line->setCwx(true);
        });
        writer.join();

        const qint64 ms = waitForMore(hpCount, before);
        QVERIFY2(ms >= 0, "no high-priority packet after the key change");
        QVERIFY2(ms < kAtOnceMs, "high priority did not go out at once");
        QTest::qWait(kSettleMs);
        const int sends = hpCount() - before;
        QVERIFY2(sends >= 1 && sends <= 2, qPrintable(QString::number(sends)));
        const QByteArray last = fake.highPriorityPackets().last();
        // dash<<2 | dot<<1 | cwx = 4 | 0 | 1.
        QCOMPARE(int(quint8(last[5])), 0x05);

        // One more change on this thread: one more send, dot now in bit 1.
        const int before2 = hpCount();
        line->setDot(true);
        QVERIFY(waitForMore(hpCount, before2) >= 0);
        QTest::qWait(kSettleMs);
        QCOMPARE(hpCount(), before2 + 1);
        QCOMPARE(int(quint8(fake.highPriorityPackets().last()[5])), 0x07);
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
        QTest::qWait(kSettleMs);
        QByteArray pkt = fake.txPackets().last();
        QCOMPARE(int(quint8(pkt[57])), 10);
        QCOMPARE(int(quint8(pkt[58])), 10);
        QCOMPARE(int(quint8(pkt[59])), 10);

        const auto txCount = [&fake]() { return int(fake.txPackets().size()); };
        int before = txCount();
        s.qskTxAttenuation = true;
        connection.setCwWireState(s);
        QVERIFY(waitForMore(txCount, before) >= 0);
        pkt = fake.txPackets().last();
        QCOMPARE(int(quint8(pkt[57])), 31);
        QCOMPARE(int(quint8(pkt[58])), 31);
        QCOMPARE(int(quint8(pkt[59])), 31);

        before = txCount();
        s.qskTxAttenuation = false;
        connection.setCwWireState(s);
        QVERIFY(waitForMore(txCount, before) >= 0);
        pkt = fake.txPackets().last();
        QCOMPARE(int(quint8(pkt[59])), 10);
    }

    // The safety rule: the cut zeroes CW enable, break-in and the key bits
    // in what is composed at once, and puts CmdTx and high priority on the
    // wire at once.
    void transmitCutZeroesCwAndSendsBothAtOnce()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection connection;
        configureConnection(connection, fake);
        QVERIFY(establish(connection, fake));
        connection.setCwWireState(keyerOn());
        connection.cwKeyLine()->setDot(true);
        connection.cwKeyLine()->setCwx(true);
        QTest::qWait(kSettleMs);
        QCOMPARE(int(quint8(fake.highPriorityPackets().last()[5])), 0x03);

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

        const qint64 txMs = waitForMore(txCount, txBefore);
        const qint64 hpMs = waitForMore(hpCount, hpBefore);
        QVERIFY2(txMs >= 0 && txMs < kAtOnceMs, "CmdTx did not go out at once");
        QVERIFY2(hpMs >= 0 && hpMs < kAtOnceMs, "high priority did not go out at once");
        for (int i = txBefore; i < txCount(); ++i) {
            QCOMPARE(int(quint8(fake.txPackets().at(i)[5])) & 0x82, 0);
        }
        for (int i = hpBefore; i < hpCount(); ++i) {
            QCOMPARE(int(quint8(fake.highPriorityPackets().at(i)[5])), 0);
            QCOMPARE(int(quint8(fake.highPriorityPackets().at(i)[4])) & 0x02, 0);
        }

        connection.clearTransmitCut();
        connection.composeCmdTxForTest(tx);
        QCOMPARE(int(tx[5] & 0x82), 0x82);
        connection.composeCmdHighPriorityForTest(hp);
        QCOMPARE(int(hp[5]), 0x03);
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
        QCoreApplication::processEvents();   // the key line's queued call
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
        QCoreApplication::processEvents();
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
