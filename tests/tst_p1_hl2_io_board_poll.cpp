// no-port-check: test-only. Upstream file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No upstream logic is ported here; this file is NereusSDR-original.
//
// Wire-byte tests for the HL2 I/O board's ongoing poll and the I2C frame
// spacing on the P1 link.
//
// The poll: mi0bot Console/console.cs:25781-25945 UpdateIOBoard [@c26a8a4],
// one step every 40 ms, switch (state++):
//   case 0:            REG_OP_MODE = the TX VFO's DSP mode, when it changed
//                      (RADE_U as DIGU, RADE_L as DIGL)
//   case 1, 4, 7, 10:  read REG_INPUT_PINS (four registers from 6)
//   case 2, 8:         ioBoard.setFrequency(TX VFO Hz): REG_TX_FREQ_BYTE4..0
//                      when the frequency changed (IoBoardHl2.cs:183-201)
//   case 3, 6:         REG_RF_INPUTS = IOBoardAerialMode, when it changed
//   case 5, 9:         REG_ANTENNA = IOBoardAerialPorts, when it changed
//   case 11:           state = 0
// and the manual I2C tool's pause (console.cs:25640-25646 SetI2CPollingPause,
// 25931-25935): the poll waits on its step while the pause is held.
// Every register goes to bus 1 (the second bus), device 0x1d
// (IoBoardHl2.cs:139, 180).
//
// An I2C frame (mi0bot ChannelMaster/networkproto1.c:903-942 [@c26a8a4]):
//   C0 = XmitBit | (0x3d << 1) | (ctrl_request << 7)   (bus 1)
//   C1 = 0x07 read, 0x06 write
//   C2 = 0x80 | address
//   C3 = register, C4 = data
// and the spacing (networkproto1.c:898-906): one I2C frame, then two
// ordinary frames before the next; the ordinary banks resume where they
// were (out_control_idx moves only in the else branch).

#include <QtTest/QtTest>
#include "core/IoBoardHl2.h"
#include "core/P1RadioConnection.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

namespace {

struct Frame {
    int c0, c1, c2, c3, c4;
};

bool isI2c(const quint8 out[5])
{
    const int chip = (out[0] >> 1) & 0x3F;
    return chip == 0x3c || chip == 0x3d;
}

} // namespace

class TestP1Hl2IoBoardPoll : public QObject {
    Q_OBJECT

private:
    // Sends subframes until the I2C queue is empty, and returns the I2C
    // frames among them.
    static QList<Frame> drain(P1RadioConnection& conn, IoBoardHl2& io)
    {
        QList<Frame> frames;
        for (int guard = 0; guard < 400 && !io.i2cQueueIsEmpty(); ++guard) {
            quint8 out[5] = {};
            conn.composeNextSubframeForTest(out);
            if (isI2c(out)) {
                frames.append({out[0], out[1], out[2], out[3], out[4]});
            }
        }
        return frames;
    }

    static Frame write(int reg, int data) { return {0x7A, 0x06, 0x9D, reg, data}; }
    static Frame readPins() { return {0xFA, 0x07, 0x9D, 6, 0}; }

    static void compare(const QList<Frame>& got, const QList<Frame>& want)
    {
        QCOMPARE(got.size(), want.size());
        for (int i = 0; i < got.size(); ++i) {
            const Frame& g = got[i];
            const Frame& w = want[i];
            QVERIFY2(g.c0 == w.c0 && g.c1 == w.c1 && g.c2 == w.c2 && g.c3 == w.c3 && g.c4 == w.c4,
                     qPrintable(QStringLiteral("frame %1: %2 %3 %4 %5 %6, want %7 %8 %9 %10 %11")
                                    .arg(i)
                                    .arg(g.c0, 2, 16).arg(g.c1, 2, 16).arg(g.c2, 2, 16)
                                    .arg(g.c3, 2, 16).arg(g.c4, 2, 16)
                                    .arg(w.c0, 2, 16).arg(w.c1, 2, 16).arg(w.c2, 2, 16)
                                    .arg(w.c3, 2, 16).arg(w.c4, 2, 16)));
        }
    }

    // One poll step and the I2C frames it sent.
    static QList<Frame> step(P1RadioConnection& conn, IoBoardHl2& io)
    {
        conn.ioBoardPollTickForTest();
        return drain(conn, io);
    }

private slots:
    void noBoardNoTraffic()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);
        for (int i = 0; i < 12; ++i) {
            conn.ioBoardPollTickForTest();
        }
        QVERIFY(io.i2cQueueIsEmpty());
    }

    void oneCycleWritesModeFrequencyAndReadsThePins()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);

        // REG_OP_MODE is 32; USB is 1.
        compare(step(conn, io), {write(32, 1)});
        compare(step(conn, io), {readPins()});
        // 14074000 Hz = 0x00 0x00 0xD6 0xC0 0x90, BYTE4 (register 0) first.
        compare(step(conn, io), {write(0, 0x00), write(1, 0x00), write(2, 0xD6),
                                 write(3, 0xC0), write(4, 0x90)});
        compare(step(conn, io), {});              // 3: aerial mode unchanged
        compare(step(conn, io), {readPins()});    // 4
        compare(step(conn, io), {});              // 5: aerial ports unchanged
        compare(step(conn, io), {});              // 6
        compare(step(conn, io), {readPins()});    // 7
        compare(step(conn, io), {});              // 8: frequency unchanged
        compare(step(conn, io), {});              // 9
        compare(step(conn, io), {readPins()});    // 10
        compare(step(conn, io), {});              // 11
        compare(step(conn, io), {});              // 0 again: mode unchanged
    }

    void changesAreWrittenOnTheirSteps()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);
        for (int i = 0; i < 12; ++i) {
            step(conn, io);
        }
        // Now at step 0 with everything written once.
        conn.setIoBoardTxState(static_cast<int>(DSPMode::LSB), 7074000);
        conn.setIoBoardAerials(2, 0x21);
        compare(step(conn, io), {write(32, 0)});              // 0: LSB
        compare(step(conn, io), {readPins()});                // 1
        compare(step(conn, io), {write(0, 0x00), write(1, 0x00), write(2, 0x6B),
                                 write(3, 0xF0), write(4, 0xD0)});   // 2
        compare(step(conn, io), {write(11, 2)});              // 3: REG_RF_INPUTS
        compare(step(conn, io), {readPins()});                // 4
        compare(step(conn, io), {write(31, 0x21)});           // 5: REG_ANTENNA
    }

    void aPinReadWaitsForAnEmptyQueue()
    {
        // mi0bot's readRequest starts a read only when the queue is empty
        // (netInterface.c:1470-1497, in_index == out_index).
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);
        conn.ioBoardPollTickForTest();   // 0: the mode write, not sent yet
        conn.ioBoardPollTickForTest();   // 1: the read must not be queued
        QCOMPARE(io.i2cQueueDepth(), 1);
    }

    void i2cFramesAreSpacedAndTheBanksResume()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);
        for (int reg = 0; reg < 3; ++reg) {
            IoBoardHl2::I2cTxn txn;
            txn.bus = 1;
            txn.address = 0x1D;
            txn.control = static_cast<quint8>(reg);
            txn.writeData = 0x55;
            QVERIFY(io.enqueueI2c(txn));
        }
        QList<int> banks;
        QString pattern;
        for (int i = 0; i < 9; ++i) {
            quint8 out[5] = {};
            conn.composeNextSubframeForTest(out);
            if (isI2c(out)) {
                pattern += QLatin1Char('I');
            } else {
                pattern += QLatin1Char('b');
                banks.append(out[0]);
            }
        }
        // networkproto1.c:898-906: I2C, two banks, I2C, two banks, ...
        QCOMPARE(pattern, QStringLiteral("IbbIbbIbb"));
        // The ordinary banks go 0, 1, 2, ... 5 with no bank lost to an I2C
        // frame (each bank's C0 as it composes on its own).
        QList<int> expected;
        for (int bank = 0; bank < 6; ++bank) {
            quint8 out[5] = {};
            conn.composeCcForBankForTest(bank, out);
            expected.append(out[0]);
        }
        QCOMPARE(banks, expected);
    }

    void radeModesWriteTheMatchingDigitalMode()
    {
        // The I/O board's REG_OP_MODE takes Thetis DSPMode values (mi0bot
        // enums.cs:252-270: DIGU 7, DIGL 9). RADE is NereusSDR's own, so
        // it is sent as the digital mode on its sideband (operator ruling
        // 2026-09-29): RADE_U as DIGU, RADE_L as DIGL.
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::RADE_U), 14236000);
        compare(step(conn, io), {write(32, 7)});
        for (int i = 0; i < 11; ++i) {
            step(conn, io);
        }
        conn.setIoBoardTxState(static_cast<int>(DSPMode::RADE_L), 7177000);
        compare(step(conn, io), {write(32, 9)});
        for (int i = 0; i < 11; ++i) {
            step(conn, io);
        }
        // Back to DIGL itself: the same value, so nothing is written.
        conn.setIoBoardTxState(static_cast<int>(DSPMode::DIGL), 7177000);
        compare(step(conn, io), {});
    }

    void aWriteDroppedByAFullQueueIsRetried()
    {
        // mi0bot I2CWrite returns -1 on a full queue (netInterface.c:
        // 1536-1564 [@c26a8a4]) and setFrequency sets currentFreq anyway
        // (IoBoardHl2.cs:183-202), so the write is lost. Here a value is
        // recorded as written only when it was queued, and the next poll on
        // that step sends it.
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);
        compare(step(conn, io), {write(32, 1)});   // 0
        compare(step(conn, io), {readPins()});     // 1
        // Fill the queue so step 2's frequency write has no room.
        for (int i = 0; i < IoBoardHl2::kMaxI2cQueue; ++i) {
            IoBoardHl2::I2cTxn txn;
            txn.bus = 1;
            txn.address = 0x1D;
            txn.control = 40;
            txn.writeData = 0x55;
            QVERIFY(io.enqueueI2c(txn));
        }
        QVERIFY(io.i2cQueueIsFull());
        conn.ioBoardPollTickForTest();             // 2: dropped
        const QList<Frame> filler = drain(conn, io);
        QCOMPARE(filler.size(), IoBoardHl2::kMaxI2cQueue);
        for (const Frame& f : filler) {
            QCOMPARE(f.c3, 40);
        }
        compare(step(conn, io), {});               // 3
        compare(step(conn, io), {readPins()});     // 4
        compare(step(conn, io), {});               // 5
        compare(step(conn, io), {});               // 6
        compare(step(conn, io), {readPins()});     // 7
        // 8: the frequency goes out now, all five bytes.
        compare(step(conn, io), {write(0, 0x00), write(1, 0x00), write(2, 0xD6),
                                 write(3, 0xC0), write(4, 0x90)});
        compare(step(conn, io), {});               // 9
    }

    void aPartlyQueuedFrequencyIsNotSplit()
    {
        // With room for only some of the five frequency bytes, none go out
        // on that step, so the board never holds half of a new frequency.
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);
        compare(step(conn, io), {write(32, 1)});   // 0
        compare(step(conn, io), {readPins()});     // 1
        for (int i = 0; i < IoBoardHl2::kMaxI2cQueue - 2; ++i) {
            IoBoardHl2::I2cTxn txn;
            txn.bus = 1;
            txn.address = 0x1D;
            txn.control = 40;
            txn.writeData = 0x55;
            QVERIFY(io.enqueueI2c(txn));
        }
        conn.ioBoardPollTickForTest();             // 2: two slots, needs five
        QCOMPARE(io.i2cQueueDepth(), IoBoardHl2::kMaxI2cQueue - 2);
        drain(conn, io);
        for (int s = 3; s < 8; ++s) {
            step(conn, io);
        }
        compare(step(conn, io), {write(0, 0x00), write(1, 0x00), write(2, 0xD6),
                                 write(3, 0xC0), write(4, 0x90)});   // 8
    }

    void aPauseHoldsThePollOnItsStep()
    {
        // mi0bot console.cs:25640-25646 SetI2CPollingPause and 25931-25935
        // [@c26a8a4]: while the manual I2C tool holds the pause, the poll
        // waits where it is (do { await Task.Delay(40); } while
        // (I2CPollingPause)); when it lets go, the poll carries on from
        // that step.
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn.setIoBoard(&io);
        conn.setIoBoardTxState(static_cast<int>(DSPMode::USB), 14074000);
        compare(step(conn, io), {write(32, 1)});   // 0
        QVERIFY(!io.isPollingPaused());
        io.setPollingPause(true);
        QVERIFY(io.isPollingPaused());
        for (int i = 0; i < 5; ++i) {
            compare(step(conn, io), {});
            QCOMPARE(io.currentStep(), 1);
        }
        io.setPollingPause(false);
        compare(step(conn, io), {readPins()});     // 1, where it stopped
        QCOMPARE(io.currentStep(), 2);
    }
};

QTEST_APPLESS_MAIN(TestP1Hl2IoBoardPoll)
#include "tst_p1_hl2_io_board_poll.moc"
