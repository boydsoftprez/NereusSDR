// no-port-check: smoke test for HL2 I2C intercept + ep6 read parsing.
// Tests the wiring between P1CodecHl2::tryComposeI2cFrame() and
// IoBoardHl2's I2C queue.
//
// Source verified against: mi0bot networkproto1.c:895-943 (intercept),
//   mi0bot networkproto1.c:478-493 (ep6 response) [@c26a8a4]

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"
#include "core/IoBoardHl2.h"
#include "core/codec/P1CodecHl2.h"

using namespace NereusSDR;

class TestP1Hl2I2cWiring : public QObject {
    Q_OBJECT
private slots:

    // Empty queue → normal compose path runs (not an I2C frame).
    // C0 bit 7 must NOT be set on bank 0 normal compose.
    void empty_queue_falls_through_to_normal_compose() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);
        // queue is empty
        QVERIFY(io.i2cQueueIsEmpty());
        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);
        // Normal bank 0 C0 should NOT have bit 7 set (that's the I2C response
        // marker; normal C0 for bank 0 is 0x00 or 0x01 depending on MOX).
        QCOMPARE(int(buf[0]) & 0x40, 0);  // I2C chip-select bits not set on normal output
    }

    // Non-empty queue → compose returns I2C TLV frame and dequeues the txn.
    // Verifies byte layout per mi0bot networkproto1.c:912-940 [@c26a8a4].
    void non_empty_queue_intercepts_compose() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);

        IoBoardHl2::I2cTxn txn;
        txn.bus       = 0;
        txn.address   = 0x1D;                     // general I/O register address
        txn.control   = IoBoardHl2::CtrlWrite | IoBoardHl2::CtrlStop;  // 0x02 (write+stop)
        txn.writeData = 0x42;
        io.enqueueI2c(txn);
        QCOMPARE(io.i2cQueueDepth(), 1);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // C0: XmitBit (0, not MOX) | (0x3c << 1) | (ctrl_request=0 << 7)
        //   = 0 | 0x78 | 0 = 0x78
        // ctrl_request bit is 0 because CtrlRequest (0x04) is not set in txn.control (0x02)
        QCOMPARE(int(buf[0]), 0x78);

        // C1 = 0x06 (write, not read — CtrlRead=0x01 not set in 0x02)
        QCOMPARE(int(buf[1]), 0x06);

        // C2 = 0x80 | 0x1D = 0x9D (stop request + address)
        QCOMPARE(int(buf[2]), 0x9D);

        // C3 = txn.control = 0x02
        QCOMPARE(int(buf[3]), int(txn.control));

        // C4 = txn.writeData = 0x42
        QCOMPARE(int(buf[4]), 0x42);

        // Txn dequeued after compose
        QCOMPARE(io.i2cQueueDepth(), 0);
    }

    // I2C bus 1 → C0 chip select uses 0x3d not 0x3c.
    // Source: mi0bot networkproto1.c:918 [@c26a8a4]: "I2C1 0x3d"
    void bus1_uses_0x3d_chip_select() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);

        IoBoardHl2::I2cTxn txn;
        txn.bus       = 1;
        txn.address   = 0x41;
        txn.control   = IoBoardHl2::CtrlRead | IoBoardHl2::CtrlStop;  // 0x03
        txn.writeData = 0x00;
        io.enqueueI2c(txn);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // C0 for bus 1 = 0 | (0x3d << 1) | 0 = 0x7a
        QCOMPARE(int(buf[0]), 0x7a);

        // C1 = 0x07 (read — CtrlRead=0x01 set in 0x03)
        QCOMPARE(int(buf[1]), 0x07);

        // C2 = 0x80 | 0x41 = 0xC1
        QCOMPARE(int(buf[2]), 0xC1);
    }

    // ctrl_request bit set → bit 7 of C0 set.
    // Source: mi0bot networkproto1.c:914 [@c26a8a4]: "(prn->i2c.ctrl_request << 7)"
    void ctrl_request_set_in_txn_control_sets_c0_bit7() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);

        IoBoardHl2::I2cTxn txn;
        txn.bus       = 0;
        txn.address   = 0x1D;
        txn.control   = IoBoardHl2::CtrlRequest;  // 0x04 — ctrl_request bit set
        txn.writeData = 0x00;
        io.enqueueI2c(txn);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // C0 = 0 | 0x78 | 0x80 = 0xF8
        QCOMPARE(int(buf[0]), 0xF8);
    }

    // Address > 0x7F → shifted right to strip r/w bit.
    // Source: mi0bot networkproto1.c:921-928 [@c26a8a4]
    void address_above_0x7F_is_right_shifted() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);

        IoBoardHl2::I2cTxn txn;
        txn.bus       = 0;
        txn.address   = 0x82;  // > 0x7F → 0x82 >> 1 = 0x41
        txn.control   = IoBoardHl2::CtrlWrite;
        txn.writeData = 0x00;
        io.enqueueI2c(txn);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // C2 = 0x80 | (0x82 >> 1) = 0x80 | 0x41 = 0xC1
        QCOMPARE(int(buf[2]), 0xC1);
    }

    // Non-HL2 board → setIoBoard is a noop, compose is unaffected even
    // if IoBoard has queued txns.
    void non_hl2_board_no_i2c_intercept() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        IoBoardHl2 io;
        conn.setIoBoard(&io);  // noop — Hermes uses P1CodecStandard, not P1CodecHl2

        IoBoardHl2::I2cTxn txn;
        txn.bus       = 0;
        txn.address   = 0x1D;
        txn.control   = IoBoardHl2::CtrlWrite;
        txn.writeData = 0xFF;
        io.enqueueI2c(txn);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);

        // Hermes emits normal bank 0 output — queue not consumed
        QCOMPARE(io.i2cQueueDepth(), 1);

        // And C0 is a normal bank 0 C0 (not an I2C frame)
        QCOMPARE(int(buf[0]) & 0x78, 0);  // 0x3c<<1 bits not set
    }

    // ep6 I2C response (bit 7 of C0 set) routes to parseI2cResponse without crash.
    // Full register routing comes in Phase 3P-E Task 3; here we verify the
    // test seam is wired and doesn't crash.
    void ep6_i2c_response_does_not_crash() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);
        // Simulate an I2C response for slot 0 with read data = 0x03
        conn.parseI2cResponseForTest(quint8(0x80 | 0), 0x03, 0x00, 0x00, 0x00);
        QVERIFY(true);  // no crash = pass
    }

    // After dequeue, second compose falls through to normal bank output.
    void after_dequeue_falls_through_to_normal() {
        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        conn.setIoBoard(&io);

        IoBoardHl2::I2cTxn txn;
        txn.bus = 0; txn.address = 0x1D;
        txn.control = IoBoardHl2::CtrlWrite; txn.writeData = 0x01;
        io.enqueueI2c(txn);

        quint8 buf[5] = {};
        conn.composeCcForBankForTest(0, buf);
        QCOMPARE(io.i2cQueueDepth(), 0);  // consumed

        quint8 buf2[5] = {};
        conn.composeCcForBankForTest(0, buf2);
        // Normal bank 0 output — C0 chip-select bits not set
        QCOMPARE(int(buf2[0]) & 0x78, 0);
    }
};

QTEST_APPLESS_MAIN(TestP1Hl2I2cWiring)
#include "tst_p1_hl2_i2c_wiring.moc"
