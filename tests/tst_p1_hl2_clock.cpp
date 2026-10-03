// no-port-check: test-only. Upstream file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No upstream logic is ported here; this file is NereusSDR-original.
//
// HL2 clock options (External 10 MHz, Enable CL2, CL2 frequency): the I2C
// writes to the clock chip at 0xd4 on bus 0, when they go, and that the
// defaults leave the wire exactly as it was before the options were wired.
// Expected tables: mi0bot Console/setup.cs:21572-21629 [@c26a8a4]; the CL2
// divider bytes from ControlCl2 (setup.cs:21694-21721) computed apart from
// the code under test, with Python's decimal module at 28 digits (C#
// Decimal's precision) following ControlCl2 step by step.

#include <QtTest/QtTest>
#include <QLoggingCategory>
#include <utility>
#include <vector>
#include "core/IoBoardHl2.h"
#include "core/P1RadioConnection.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

namespace {

using Writes = std::vector<std::pair<int, int>>;


// The 96 subframes an HL2 with the I/O board sends from a fresh start, one
// I/O board poll step every third subframe, captured from the sources
// before the clock options were wired (22bcd02f9). Bank 10's C2 (the four
// 12 00 49 80 00 subframes) carries mic_boost in bit 0 (Thetis
// ChannelMaster/networkproto1.c:581 [v2.10.3.15]); it reads 0x49, not the
// captured 0x48, since the connection's mic boost starts on as Thetis's does
// (console.cs:13259 [v2.10.3.15]: private bool mic_boost = true;). No other
// byte changed.
const char* const kGoldenDefaultWireHex =
    "7a069d200100000018040200000000fa079d0600040000000006000000007a069d00001c04000000"
    "08000000007a069d01000a000000000c000000007a069d02d60e0000000010000000007a069d03c0"
    "1200498000140000005f7a069d049016202000001e00000000fa079d060020000000002200000000"
    "24000000002e00000c147400000000000000180402000000000400000000fa079d06000600000000"
    "1c0400000008000000000a000000000c000000000e0000000010000000001200498000fa079d0600"
    "140000005f16202000001e000000002000000000220000000024000000002e00000c147400000000"
    "fa079d060000000018040200000000040000000006000000001c0400000008000000000a00000000"
    "0c00000000fa079d06000e0000000010000000001200498000140000005f16202000001e00000000"
    "20000000002200000000fa079d060024000000002e00000c14740000000000000018040200000000"
    "040000000006000000001c04000000fa079d060008000000000a000000000c000000000e00000000"
    "10000000001200498000140000005f1620200000fa079d06001e0000000020000000002200000000"
    "24000000002e00000c14740000000000000018040200000000fa079d060004000000000600000000";

const Writes k10MhzEnable = {
    {0x10, 0xc0}, {0x13, 0x03}, {0x10, 0x40}, {0x2d, 0x01}, {0x2e, 0x20},
    {0x22, 0x03}, {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x19, 0x00},
    {0x1A, 0x00}, {0x1B, 0x00}, {0x18, 0x00}, {0x17, 0x12}};
const Writes k10MhzDisable = {
    {0x10, 0xc0}, {0x13, 0x00}, {0x10, 0x80}, {0x2d, 0x01}, {0x2e, 0x10},
    {0x22, 0x00}, {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x19, 0x00},
    {0x1A, 0x00}, {0x1B, 0x00}, {0x18, 0x40}, {0x17, 0x04}};
const Writes kCl2Off = {
    {0x62, 0x5b}, {0x2c, 0x00}, {0x31, 0x00}, {0x3d, 0x00}, {0x3e, 0x00},
    {0x32, 0x00}, {0x33, 0x00}, {0x34, 0x00}, {0x35, 0x00}, {0x63, 0x00}};

// The CL2 table with the six divider bytes for registers 0x3d, 0x3e,
// 0x32, 0x33, 0x34 and 0x35.
Writes cl2(int d3d, int d3e, int d32, int d33, int d34, int d35)
{
    return {{0x62, 0x3b}, {0x2c, 0x00}, {0x31, 0x81}, {0x3d, d3d}, {0x3e, d3e},
            {0x32, d32}, {0x33, d33}, {0x34, d34}, {0x35, d35}, {0x63, 0x01}};
}

Writes concat(Writes a, const Writes& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

} // namespace

class TestP1Hl2Clock : public QObject {
    Q_OBJECT

private:
    static QByteArray wireOf(P1RadioConnection& conn)
    {
        QByteArray wire;
        for (int i = 0; i < 96; ++i) {
            if (i % 3 == 0) {
                conn.ioBoardPollTickForTest();
            }
            quint8 out[5] = {};
            conn.composeNextSubframeForTest(out);
            wire.append(reinterpret_cast<const char*>(out), 5);
        }
        return wire;
    }

    static void setUpHl2(P1RadioConnection& conn, IoBoardHl2& io)
    {
        conn.setBoardForTest(HPSDRHW::HermesLite);
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);
    }

    // Everything in the I2C queue, which must all be clock chip writes.
    static Writes drainClockWrites(IoBoardHl2& io)
    {
        Writes writes;
        IoBoardHl2::I2cTxn txn;
        while (io.dequeueI2c(txn)) {
            if (txn.bus != 0 || txn.address != 0xd4 || txn.isRead || txn.needsResponse) {
                qWarning("unexpected I2C transaction bus=%d addr=0x%02x", txn.bus,
                         txn.address);
                return {};
            }
            writes.emplace_back(txn.control, txn.writeData);
        }
        return writes;
    }

    // The clock chip writes left in the I2C queue, filler skipped.
    static Writes drainQueueClockWrites(IoBoardHl2& io)
    {
        Writes writes;
        IoBoardHl2::I2cTxn txn;
        while (io.dequeueI2c(txn)) {
            if (txn.address == 0xd4) {
                writes.emplace_back(txn.control, txn.writeData);
            }
        }
        return writes;
    }

    // Two ep6 frames: the first finds the lists' last writes gone from the
    // queue, the second shows the radio still answering after them.
    static void radioAnswers(P1RadioConnection& conn)
    {
        conn.hl2ClockEp6ForTest();
        conn.hl2ClockEp6ForTest();
    }

    static void fillQueue(IoBoardHl2& io)
    {
        IoBoardHl2::I2cTxn filler;
        filler.bus = 1;
        filler.address = 0x1d;
        while (io.enqueueI2c(filler)) {
        }
    }

private slots:
    void initTestCase()
    {
        // The I2C frame, TX-edge and codec-choice lines are not part of this test.
        QLoggingCategory::setFilterRules(
            QStringLiteral("*.debug=false\n*.info=false\ndefault.debug=false"));
    }

    // With the options at their defaults nothing on the wire changes.
    void defaultsKeepTodaysWire()
    {
        const QByteArray golden = QByteArray::fromHex(kGoldenDefaultWireHex);
        QCOMPARE(golden.size(), 96 * 5);
        {
            P1RadioConnection conn;
            IoBoardHl2 io;
            setUpHl2(conn, io);
            QCOMPARE(wireOf(conn).toHex(), golden.toHex());
        }
        {
            // The defaults handed over before connect, then data flows.
            P1RadioConnection conn;
            IoBoardHl2 io;
            setUpHl2(conn, io);
            conn.setHl2Clock(false, false, 116000);
            conn.simulateDataFlowingForTest();
            QCOMPARE(conn.hl2ClockPendingForTest(), 0);
            QCOMPARE(wireOf(conn).toHex(), golden.toHex());
        }
        {
            // Connected, the defaults handed over again (any other HL2
            // option change does this): still nothing sent.
            P1RadioConnection conn;
            IoBoardHl2 io;
            setUpHl2(conn, io);
            conn.simulateDataFlowingForTest();
            conn.setHl2Clock(false, false, 116000);
            QCOMPARE(wireOf(conn).toHex(), golden.toHex());
        }
    }

    // The frames on the wire: bus 0 (C0 0x3c << 1), write (C1 0x06),
    // 0x80 | 0xd4 >> 1 (C2), register (C3), data (C4).
    // From mi0bot networkproto1.c:912-939 [@c26a8a4] and setup.cs:21647.
    void ext10MHzOnAtConnectWireBytes()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.setHl2Clock(true, false, 116000);
        QCOMPARE(io.i2cQueueDepth(), 0);   // stored only until data flows
        conn.simulateDataFlowingForTest();
        const QByteArray wire = wireOf(conn);
        QByteArray clockFrames;
        for (int i = 0; i + 5 <= wire.size(); i += 5) {
            if (quint8(wire[i]) == 0x78) {
                clockFrames.append(wire.mid(i, 5));
            }
        }
        QByteArray expected;
        for (const auto& [reg, data] : k10MhzEnable) {
            const char frame[5] = {char(0x78), char(0x06), char(0xEA), char(reg), char(data)};
            expected.append(frame, 5);
        }
        QCOMPARE(clockFrames.toHex(), expected.toHex());
    }

    void cl2OnAtConnect()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.setHl2Clock(false, true, 116000);
        conn.simulateDataFlowingForTest();
        // VCO 1305.6 MHz / 116: integer 11, fraction 4281082 / 2^24.
        QCOMPARE(drainClockWrites(io), cl2(0x00, 0xB0, 0x01, 0x05, 0x4B, 0xE0));
    }

    void bothOnAtConnect()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.setHl2Clock(true, true, 116000);
        conn.simulateDataFlowingForTest();
        // With the external reference the VCO is 1440 MHz: /116 is integer
        // 12, fraction 6942296 / 2^24.
        QCOMPARE(drainClockWrites(io),
                 concat(k10MhzEnable, cl2(0x00, 0xC0, 0x01, 0xA7, 0xB9, 0x60)));
    }

    // A reconnect after a lost link sends the options that are on again.
    void reconnectSendsAgain()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.setHl2Clock(false, true, 116000);
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io).size(), std::size_t(10));
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), cl2(0x00, 0xB0, 0x01, 0x05, 0x4B, 0xE0));
    }

    // Live changes, as mi0bot's three handlers send them.
    void liveChanges()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), Writes{});

        // External 10 MHz on: its table, then ControlCl2(false) = CL2 off.
        conn.setHl2Clock(true, false, 116000);
        QCOMPARE(drainClockWrites(io), concat(k10MhzEnable, kCl2Off));

        // CL2 on at 116 MHz on the 1440 MHz VCO.
        conn.setHl2Clock(true, true, 116000);
        QCOMPARE(drainClockWrites(io), cl2(0x00, 0xC0, 0x01, 0xA7, 0xB9, 0x60));

        // A frequency change with CL2 on: the new divider (1440 / 200).
        conn.setHl2Clock(true, true, 200000);
        QCOMPARE(drainClockWrites(io), cl2(0, 112, 0, 204, 204, 196));

        // CL2 off.
        conn.setHl2Clock(true, false, 200000);
        QCOMPARE(drainClockWrites(io), kCl2Off);

        // A frequency change with CL2 off still sends CL2 off, as
        // udCl2Freq_ValueChanged calls ControlCl2(false).
        conn.setHl2Clock(true, false, 10000);
        QCOMPARE(drainClockWrites(io), kCl2Off);

        // External 10 MHz off: its off table, then CL2 off.
        conn.setHl2Clock(false, false, 10000);
        QCOMPARE(drainClockWrites(io), concat(k10MhzDisable, kCl2Off));

        // Turning External 10 MHz off with CL2 on resends CL2 on the
        // internal VCO: 1305.6 / 10.
        conn.setHl2Clock(true, true, 10000);
        drainClockWrites(io);
        conn.setHl2Clock(false, true, 10000);
        QCOMPARE(drainClockWrites(io), concat(k10MhzDisable, cl2(8, 32, 2, 61, 112, 160)));

        // The same values again send nothing.
        conn.setHl2Clock(false, true, 10000);
        QCOMPARE(drainClockWrites(io), Writes{});
    }

    // From mi0bot setup.designer.cs:11133-11163 [@c26a8a4] udCl2Freq: 1..200 MHz.
    void frequencyClamps()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, true, 0);
        QCOMPARE(conn.hl2Cl2FreqKHzForTest(), 1000);
        QCOMPARE(drainClockWrites(io), cl2(81, 144, 2, 102, 102, 100));
        conn.setHl2Clock(false, true, 500000);
        QCOMPARE(conn.hl2Cl2FreqKHzForTest(), 200000);
        QCOMPARE(drainClockWrites(io), cl2(0, 96, 2, 28, 172, 0));
        conn.setHl2Clock(true, true, -3000);
        QCOMPARE(conn.hl2Cl2FreqKHzForTest(), 1000);
        QCOMPARE(drainClockWrites(io), concat(k10MhzEnable, cl2(90, 0, 0, 0, 0, 0)));
    }

    // mi0bot masks the last divider byte with 0xf6 (setup.cs:21718
    // [@c26a8a4]). At 3 MHz the unmasked byte would be 0xCC.
    void lastDividerByteIsMasked()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, true, 3000);
        QCOMPARE(drainClockWrites(io), cl2(0x1B, 0x30, 0x00, 0xCC, 0xCC, 0xC4));
    }

    // Frequencies with decimals, as udCl2Freq holds three places
    // (setup.designer.cs:11133-11163 [@c26a8a4]). Expected bytes from
    // Python's decimal module at 28 digits following ControlCl2, not from
    // the integer arithmetic under test.
    void fractionalFrequencies_data()
    {
        QTest::addColumn<bool>("ext");
        QTest::addColumn<int>("kHz");
        QTest::addColumn<Writes>("expected");
        QTest::newRow("24.576 internal") << false << 24576 << cl2(0x03, 0x50, 0x00, 0x80, 0x00, 0x00);
        QTest::newRow("24.576 external") << true  << 24576 << cl2(0x03, 0xA0, 0x02, 0x60, 0x00, 0x00);
        QTest::newRow("10.7 internal")   << false << 10700 << cl2(0x07, 0xA0, 0x00, 0x13, 0x23, 0xE0);
        QTest::newRow("10.7 external")   << true  << 10700 << cl2(0x08, 0x60, 0x02, 0x51, 0x58, 0x84);
        QTest::newRow("1.001 internal")  << false << 1001  << cl2(0x51, 0x80, 0x01, 0x2E, 0xCD, 0x10);
        QTest::newRow("199.999 internal") << false << 199999 << cl2(0x00, 0x60, 0x02, 0x1C, 0xB4, 0x94);
        QTest::newRow("116.5 internal")  << false << 116500 << cl2(0x00, 0xB0, 0x00, 0xD3, 0xD4, 0xE4);
        QTest::newRow("116.5 external")  << true  << 116500 << cl2(0x00, 0xC0, 0x01, 0x71, 0x2A, 0xD0);
    }

    void fractionalFrequencies()
    {
        QFETCH(bool, ext);
        QFETCH(int, kHz);
        QFETCH(Writes, expected);
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.setHl2Clock(ext, true, kHz);
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), ext ? concat(k10MhzEnable, expected) : expected);
    }

    void nothingSentWhileDisconnected()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.setHl2Clock(true, true, 50000);
        conn.setHl2Clock(false, true, 60000);
        QCOMPARE(io.i2cQueueDepth(), 0);
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);
    }

    void nothingSentOnOtherBoards()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Hermes);
        IoBoardHl2 io;
        conn.setIoBoard(&io);
        conn.setHl2Clock(true, true, 116000);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, false, 20000);
        QCOMPARE(io.i2cQueueDepth(), 0);
    }

    void disconnectDropsPendingWrites()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        fillQueue(io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(true, false, 116000);
        QCOMPARE(conn.hl2ClockPendingForTest(), 24);
        conn.disconnect();
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);
        // Both lists were cut short, so both go again at the next connect.
        QVERIFY(conn.hl2ClockIncompleteForTest(true));
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
    }

    // A full I2C queue with nothing leaving it is given up after 50
    // attempts in a row, mi0bot's Timeout of 50 (setup.cs:21658 [@c26a8a4]).
    void stalledQueueGivesUpAfterFifty()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        fillQueue(io);
        conn.setHl2Clock(true, false, 116000);   // attempt 1 of the 10 MHz list
        QCOMPARE(conn.hl2ClockPendingForTest(), 24);
        for (int i = 0; i < 48; ++i) {           // attempts 2..49
            conn.hl2ClockPumpForTest();
        }
        QCOMPARE(conn.hl2ClockPendingForTest(), 24);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("clock chip I2C writes timed out")));
        conn.hl2ClockPumpForTest();               // attempt 50: the list is dropped
        QCOMPARE(conn.hl2ClockPendingForTest(), 10);
        QVERIFY(conn.hl2ClockIncompleteForTest(true));

        // Room in the queue: the retry timer sends the CL2 off list.
        IoBoardHl2::I2cTxn txn;
        while (io.dequeueI2c(txn)) {
        }
        QTRY_COMPARE(conn.hl2ClockPendingForTest(), 0);
        QCOMPARE(drainClockWrites(io), kCl2Off);
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
    }

    // A queue that moves between attempts starts the count again: 49
    // stalled attempts, one write leaves the queue, 49 more stalled
    // attempts, and the list is still there.
    void drainResetsTheBudget()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        fillQueue(io);
        conn.setHl2Clock(true, false, 116000);   // attempt 1
        for (int i = 0; i < 48; ++i) {           // attempts 2..49
            conn.hl2ClockPumpForTest();
        }
        IoBoardHl2::I2cTxn txn;
        QVERIFY(io.dequeueI2c(txn));              // one slot frees
        QVERIFY(io.enqueueI2c(txn));              // and fills again elsewhere
        for (int i = 0; i < 49; ++i) {
            conn.hl2ClockPumpForTest();
        }
        QCOMPARE(conn.hl2ClockPendingForTest(), 24);
        QVERIFY(!conn.hl2ClockIncompleteForTest(true));
    }

    // Writes that go in are not counted: a list longer than 50 writes'
    // worth of attempts, each write finding room only after a few full
    // tries, still ends complete.
    void successesAreNotCounted()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        fillQueue(io);
        conn.setHl2Clock(true, true, 116000);    // 14 + 10 writes waiting
        IoBoardHl2::I2cTxn txn;
        int attempts = 1;
        Writes sent;
        for (int round = 0; round < 200 && conn.hl2ClockPendingForTest() > 0; ++round) {
            for (int i = 0; i < 3; ++i) {         // three full tries per write
                conn.hl2ClockPumpForTest();
                ++attempts;
            }
            QVERIFY(io.dequeueI2c(txn));          // the queue moves by one
            if (txn.address == 0xd4) {
                sent.emplace_back(txn.control, txn.writeData);
            }
            conn.hl2ClockPumpForTest();
            ++attempts;
        }
        QVERIFY(attempts > 51);
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);
        sent = concat(sent, drainQueueClockWrites(io));
        QCOMPARE(sent, concat(k10MhzEnable, cl2(0x00, 0xC0, 0x01, 0xA7, 0xB9, 0x60)));
        QVERIFY(!conn.hl2ClockIncompleteForTest(true));
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
    }

    // Three quick External 10 MHz changes while the queue drains one write
    // at a time: the lists that had not started are rebuilt, and the last
    // change lands complete, with no time-out.
    void rapidTogglesEndOnTheLast()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        fillQueue(io);
        conn.setHl2Clock(true, false, 116000);   // on: nothing in yet
        IoBoardHl2::I2cTxn txn;
        Writes sent;
        const auto drainOne = [&]() {
            QVERIFY(io.dequeueI2c(txn));
            if (txn.address == 0xd4) {
                sent.emplace_back(txn.control, txn.writeData);
            }
            conn.hl2ClockPumpForTest();
        };
        drainOne();                               // the first on write goes in
        conn.setHl2Clock(false, false, 116000);  // off
        drainOne();
        conn.setHl2Clock(true, false, 116000);   // on again
        for (int round = 0; round < 200 && conn.hl2ClockPendingForTest() > 0; ++round) {
            drainOne();
            conn.hl2ClockPumpForTest();
            conn.hl2ClockPumpForTest();
        }
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);
        sent = concat(sent, drainQueueClockWrites(io));
        // The on list that had started runs to its end; the off list never
        // started and is dropped, since the last change wants on again;
        // then the CL2 off list the last change needs.
        QCOMPARE(sent, concat(k10MhzEnable, kCl2Off));
        QVERIFY(!conn.hl2ClockIncompleteForTest(true));
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
    }

    // A frequency changed twice before its list starts sends only the last.
    void unstartedListIsRebuilt()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        fillQueue(io);
        conn.setHl2Clock(false, true, 116000);
        conn.setHl2Clock(false, true, 10700);
        conn.setHl2Clock(false, true, 24576);
        QCOMPARE(conn.hl2ClockPendingForTest(), 10);
        IoBoardHl2::I2cTxn txn;
        while (io.dequeueI2c(txn)) {
        }
        conn.hl2ClockPumpForTest();
        QCOMPARE(drainClockWrites(io), cl2(0x03, 0x50, 0x00, 0x80, 0x00, 0x00));
    }

    // A list that did not finish is sent again at the next connect, from
    // the value wanted then, even when that value is off.
    void incompleteListResentAtConnect()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.setHl2Clock(true, false, 116000);
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), k10MhzEnable);
        fillQueue(io);
        conn.setHl2Clock(false, false, 116000);  // off, cut short by a disconnect
        conn.disconnect();
        QVERIFY(conn.hl2ClockIncompleteForTest(true));
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
        IoBoardHl2::I2cTxn txn;
        while (io.dequeueI2c(txn)) {
        }
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), concat(k10MhzDisable, kCl2Off));
        // Out of the queue, but not finished until the radio answers.
        QVERIFY(conn.hl2ClockIncompleteForTest(true));
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
        radioAnswers(conn);
        QVERIFY(!conn.hl2ClockIncompleteForTest(true));
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
        // Finished: the connect after that sends nothing for options off.
        conn.disconnect();
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), Writes{});
    }

    // A list whose writes all went into the queue but never left it before
    // the disconnect is sent again at the next connect.
    void queuedListResentAfterDisconnect()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, true, 116000);
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);   // all 10 writes queued
        conn.hl2ClockEp6ForTest();                     // still in the queue
        conn.hl2ClockEp6ForTest();
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
        conn.setHl2Clock(false, false, 116000);       // off, queued behind it
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);
        conn.disconnect();
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
        QVERIFY(!conn.hl2ClockIncompleteForTest(true));
        // The frames the disconnect left in the queue go first, then the
        // off list again, as the connect wants CL2 off.
        const Writes stale = drainClockWrites(io);
        QCOMPARE(stale, concat(cl2(0x00, 0xB0, 0x01, 0x05, 0x4B, 0xE0), kCl2Off));
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), kCl2Off);
        radioAnswers(conn);
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
    }

    // A list that left the queue while the radio had stopped answering
    // (the watchdog's silence) is sent again when the link comes back.
    void listSentInSilenceResentAfterLinkLost()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(true, false, 116000);
        QCOMPARE(drainClockWrites(io), concat(k10MhzEnable, kCl2Off));
        // No ep6 frame after the writes left: the link is declared lost.
        conn.hl2ClockLinkLostForTest();
        QVERIFY(conn.hl2ClockIncompleteForTest(true));
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), concat(k10MhzEnable, kCl2Off));
        // One ep6 frame that finds the writes gone is not enough; a lost
        // link then still sends them again.
        conn.hl2ClockEp6ForTest();
        conn.hl2ClockLinkLostForTest();
        QVERIFY(conn.hl2ClockIncompleteForTest(true));
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), concat(k10MhzEnable, kCl2Off));
        radioAnswers(conn);
        QVERIFY(!conn.hl2ClockIncompleteForTest(true));
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
        // Answered: a lost link now leaves nothing to send but the option
        // that is on.
        conn.hl2ClockLinkLostForTest();
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), k10MhzEnable);
    }

    // A later list of the same kind that is dropped keeps its flag, even
    // when an earlier list of that kind is answered afterwards.
    void droppedListNotClearedByEarlierAnswer()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, true, 116000);        // queued whole
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);
        QCOMPARE(drainClockWrites(io).size(), std::size_t(10));   // and gone
        fillQueue(io);
        conn.setHl2Clock(false, true, 24576);         // attempt 1, no room
        for (int i = 0; i < 48; ++i) {
            conn.hl2ClockPumpForTest();
        }
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("clock chip I2C writes timed out")));
        conn.hl2ClockPumpForTest();                    // attempt 50: dropped
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
        radioAnswers(conn);
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
    }

    // The manual I2C tool's Clear Queue drops a clock list queued whole
    // before it left: it never counts as sent, and goes again at once.
    void clearedQueuedListIsResent()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, true, 116000);
        QCOMPARE(conn.hl2ClockPendingForTest(), 0);   // all 10 writes queued
        io.clearI2cQueue();
        radioAnswers(conn);
        QVERIFY(conn.hl2ClockIncompleteForTest(false));
        QCOMPARE(drainClockWrites(io), cl2(0x00, 0xB0, 0x01, 0x05, 0x4B, 0xE0));
        radioAnswers(conn);
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
        QCOMPARE(drainClockWrites(io), Writes{});
    }

    // A clear in the middle of a list: the writes already in the queue are
    // gone, so the list starts again from its first write.
    void clearedStartedListStartsAgain()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        fillQueue(io);
        conn.setHl2Clock(true, false, 116000);
        IoBoardHl2::I2cTxn txn;
        QVERIFY(io.dequeueI2c(txn));                  // room for one write
        conn.hl2ClockPumpForTest();                    // the first goes in
        QCOMPARE(conn.hl2ClockPendingForTest(), 23);
        io.clearI2cQueue();
        conn.hl2ClockPumpForTest();
        QCOMPARE(drainClockWrites(io), concat(k10MhzEnable, kCl2Off));
        radioAnswers(conn);
        QVERIFY(!conn.hl2ClockIncompleteForTest(true));
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
    }

    // A clear after a list's last write left the queue drops nothing of
    // it; the list is not sent again.
    void clearAfterListLeftResendsNothing()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, true, 116000);
        QCOMPARE(drainClockWrites(io).size(), std::size_t(10));
        conn.hl2ClockEp6ForTest();                     // the last write left
        io.clearI2cQueue();
        radioAnswers(conn);
        QCOMPARE(drainClockWrites(io), Writes{});
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
    }

    // A clear while the link is down is left to the next connect, which
    // sends the list again; a later ep6 frame does not send it twice.
    void clearWhileDisconnectedResentOnceAtConnect()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        conn.setHl2Clock(false, true, 116000);
        conn.disconnect();
        io.clearI2cQueue();
        conn.simulateDataFlowingForTest();
        QCOMPARE(drainClockWrites(io), cl2(0x00, 0xB0, 0x01, 0x05, 0x4B, 0xE0));
        radioAnswers(conn);
        QCOMPARE(drainClockWrites(io), Writes{});
        QVERIFY(!conn.hl2ClockIncompleteForTest(false));
    }

    // The I/O board poll waits while clock writes are pending, as mi0bot
    // holds SetI2CPollingPause around WriteVersaClockAsync.
    void pollWaitsWhileClockWritesPending()
    {
        P1RadioConnection conn;
        IoBoardHl2 io;
        setUpHl2(conn, io);
        conn.simulateDataFlowingForTest();
        fillQueue(io);
        conn.setHl2Clock(true, false, 116000);
        const int step = io.currentStep();
        conn.ioBoardPollTickForTest();
        QCOMPARE(io.currentStep(), step);
        conn.disconnect();
        conn.ioBoardPollTickForTest();
        QVERIFY(io.currentStep() != step);
        QVERIFY(!io.isPollingPaused());
    }
};

QTEST_MAIN(TestP1Hl2Clock)
#include "tst_p1_hl2_clock.moc"
