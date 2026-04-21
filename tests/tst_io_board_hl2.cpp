// no-port-check: test fixture asserts IoBoardHl2 model behavior — I2C queue + state machine + registers
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/IoBoardHl2.h"

using namespace NereusSDR;

class TestIoBoardHl2 : public QObject {
    Q_OBJECT
private slots:
    // ── I2C queue ──

    void queue_starts_empty() {
        IoBoardHl2 io;
        QCOMPARE(io.i2cQueueDepth(), 0);
        QVERIFY(io.i2cQueueIsEmpty());
        QVERIFY(!io.i2cQueueIsFull());
    }

    void enqueue_increments_depth() {
        IoBoardHl2 io;
        IoBoardHl2::I2cTxn txn{
            .bus       = 0,
            .address   = 0x20,
            .control   = IoBoardHl2::CtrlStop,
            .writeData = 0x42,
        };
        QVERIFY(io.enqueueI2c(txn));
        QCOMPARE(io.i2cQueueDepth(), 1);
    }

    void enqueue_full_returns_false() {
        IoBoardHl2 io;
        IoBoardHl2::I2cTxn txn{};
        for (int i = 0; i < 32; ++i) { QVERIFY(io.enqueueI2c(txn)); }
        // 33rd should fail
        QVERIFY(!io.enqueueI2c(txn));
        QCOMPARE(io.i2cQueueDepth(), 32);
        QVERIFY(io.i2cQueueIsFull());
    }

    void dequeue_returns_oldest() {
        IoBoardHl2 io;
        IoBoardHl2::I2cTxn txnA{.address = 0x10, .writeData = 0xA};
        IoBoardHl2::I2cTxn txnB{.address = 0x20, .writeData = 0xB};
        io.enqueueI2c(txnA);
        io.enqueueI2c(txnB);
        IoBoardHl2::I2cTxn out{};
        QVERIFY(io.dequeueI2c(out));
        QCOMPARE(int(out.address), 0x10);
        QVERIFY(io.dequeueI2c(out));
        QCOMPARE(int(out.address), 0x20);
        QVERIFY(!io.dequeueI2c(out));  // queue now empty
    }

    void clear_queue_resets_depth() {
        IoBoardHl2 io;
        IoBoardHl2::I2cTxn txn{};
        io.enqueueI2c(txn);
        io.enqueueI2c(txn);
        io.clearI2cQueue();
        QCOMPARE(io.i2cQueueDepth(), 0);
        QVERIFY(io.i2cQueueIsEmpty());
    }

    // ── 12-step state machine ──

    void state_machine_starts_at_step0() {
        IoBoardHl2 io;
        QCOMPARE(io.currentStep(), 0);
    }

    void advance_step_cycles_through_12() {
        IoBoardHl2 io;
        for (int i = 0; i < 12; ++i) {
            QCOMPARE(io.currentStep(), i);
            io.advanceStep();
        }
        // After step 11 advances, wraps back to 0
        QCOMPARE(io.currentStep(), 0);
    }

    void step_descriptors_match_thetis() {
        IoBoardHl2 io;
        // Verified against mi0bot console.cs:25844-25928 switch(state++) [@c26a8a4]
        QCOMPARE(io.stepDescriptor(0),  QStringLiteral("WR REG_OP_MODE"));
        QCOMPARE(io.stepDescriptor(1),  QStringLiteral("RD REG_INPUT_PINS"));
        QCOMPARE(io.stepDescriptor(2),  QStringLiteral("WR REG_FREQUENCY"));
        QCOMPARE(io.stepDescriptor(3),  QStringLiteral("WR REG_RF_INPUTS"));
        QCOMPARE(io.stepDescriptor(4),  QStringLiteral("RD REG_INPUT_PINS"));
        QCOMPARE(io.stepDescriptor(5),  QStringLiteral("WR REG_ANTENNA"));
        QCOMPARE(io.stepDescriptor(6),  QStringLiteral("WR REG_RF_INPUTS"));
        QCOMPARE(io.stepDescriptor(7),  QStringLiteral("RD REG_INPUT_PINS"));
        QCOMPARE(io.stepDescriptor(8),  QStringLiteral("WR REG_FREQUENCY"));
        QCOMPARE(io.stepDescriptor(9),  QStringLiteral("WR REG_ANTENNA"));
        QCOMPARE(io.stepDescriptor(10), QStringLiteral("RD REG_INPUT_PINS"));
        QCOMPARE(io.stepDescriptor(11), QStringLiteral("CYCLE"));
    }

    void step_descriptor_out_of_range_returns_question_mark() {
        IoBoardHl2 io;
        QCOMPARE(io.stepDescriptor(-1), QStringLiteral("?"));
        QCOMPARE(io.stepDescriptor(12), QStringLiteral("?"));
    }

    // ── Register state ──

    void registers_default_zero() {
        IoBoardHl2 io;
        QCOMPARE(int(io.registerValue(IoBoardHl2::Register::HardwareVersion)), 0);
        QCOMPARE(int(io.registerValue(IoBoardHl2::Register::REG_OP_MODE)), 0);
        QCOMPARE(int(io.registerValue(IoBoardHl2::Register::REG_FAULT)), 0);
    }

    void setRegister_emits_signal() {
        IoBoardHl2 io;
        QSignalSpy spy(&io, &IoBoardHl2::registerChanged);
        io.setRegisterValue(IoBoardHl2::Register::HardwareVersion, 0xF1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(int(io.registerValue(IoBoardHl2::Register::HardwareVersion)), 0xF1);
    }

    void setRegister_no_signal_if_unchanged() {
        IoBoardHl2 io;
        io.setRegisterValue(IoBoardHl2::Register::REG_CONTROL, 0x01);
        QSignalSpy spy(&io, &IoBoardHl2::registerChanged);
        io.setRegisterValue(IoBoardHl2::Register::REG_CONTROL, 0x01);  // same value
        QCOMPARE(spy.count(), 0);
    }

    // ── Detection ──

    void detected_default_false() {
        IoBoardHl2 io;
        QVERIFY(!io.isDetected());
    }

    void setDetected_emits_signal() {
        IoBoardHl2 io;
        QSignalSpy spy(&io, &IoBoardHl2::detectedChanged);
        io.setDetected(true);
        QCOMPARE(spy.count(), 1);
        QVERIFY(io.isDetected());
    }

    void setDetected_no_signal_if_unchanged() {
        IoBoardHl2 io;
        io.setDetected(true);
        QSignalSpy spy(&io, &IoBoardHl2::detectedChanged);
        io.setDetected(true);  // same value
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_APPLESS_MAIN(TestIoBoardHl2)
#include "tst_io_board_hl2.moc"
