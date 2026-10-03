// no-port-check: test fixture asserts IoBoardHl2 model behavior — I2C queue + state machine + registers
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/IoBoardHl2.h"

#include <thread>
#include <vector>

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
            .control   = 0x00,   // sub-address (test only checks queue depth)
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

    // ── Unanswered reads ──

    // A read the radio never answers must not take the next read's answer
    // (mi0bot gives it up after its 20 one-millisecond polls, and the next
    // read takes the answer slot).
    void unansweredRead_isGivenUpWhenTheNextReadGoesOut() {
        IoBoardHl2 io;
        qint64 now = 1000;
        io.setClockForTest([&now]() { return now; });
        QVERIFY(io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral, 6}));
        now += IoBoardHl2::kReadAnswerMs;
        QVERIFY(io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral, 169}));
        QSignalSpy answered(&io, &IoBoardHl2::i2cReadAnswered);
        io.applyI2cReadResponse(0x80 | (0x3d << 1), 0x00, 0x00, 0x00, 0x04);
        QCOMPARE(answered.count(), 1);
        QCOMPARE(answered.at(0).at(1).value<quint8>(), quint8(169));
        QCOMPARE(io.registerValue(IoBoardHl2::Register::REG_OUT_PINS), quint8(0x04));
        QCOMPARE(io.pendingReadDepth(), 0);
    }

    // Reads still inside their answer time keep their order.
    void readsInsideTheirAnswerTime_keepTheirOrder() {
        IoBoardHl2 io;
        qint64 now = 1000;
        io.setClockForTest([&now]() { return now; });
        QVERIFY(io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral, 9}));
        now += IoBoardHl2::kReadAnswerMs - 1;
        QVERIFY(io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral, 10}));
        QSignalSpy answered(&io, &IoBoardHl2::i2cReadAnswered);
        io.applyI2cReadResponse(0x80 | (0x3d << 1), 0x00, 0x00, 0x00, 0x01);
        io.applyI2cReadResponse(0x80 | (0x3d << 1), 0x00, 0x00, 0x00, 0x02);
        QCOMPARE(answered.count(), 2);
        QCOMPARE(answered.at(0).at(1).value<quint8>(), quint8(9));
        QCOMPARE(answered.at(1).at(1).value<quint8>(), quint8(10));
    }

    // A late answer with no newer read waiting still lands on its read.
    void lateAnswer_withNoNewerRead_landsOnItsRead() {
        IoBoardHl2 io;
        qint64 now = 1000;
        io.setClockForTest([&now]() { return now; });
        QVERIFY(io.pushPendingRead({IoBoardHl2::kI2cAddrGeneral, 6}));
        now += 10 * IoBoardHl2::kReadAnswerMs;
        QSignalSpy answered(&io, &IoBoardHl2::i2cReadAnswered);
        io.applyI2cReadResponse(0x80 | (0x3d << 1), 0x00, 0x00, 0x00, 0x08);
        QCOMPARE(answered.count(), 1);
        QCOMPARE(answered.at(0).at(1).value<quint8>(), quint8(6));
    }

    // ── Threads ──

    // The main thread (the I2C tool) and the connection thread (the poll)
    // both queue transactions while the codec takes them on the connection
    // thread. Every transaction must come out once, each producer's in the
    // order it queued them, and the depth must stay in range. No clock: the
    // consumer runs until it has every transaction, so load only slows it,
    // and a lost transaction shows as the ctest time limit.
    void i2cQueue_isSafeAcrossThreads() {
        IoBoardHl2 io;
        constexpr int kPerProducer = 4000;
        const auto produce = [&io](quint8 producer) {
            for (int i = 0; i < kPerProducer;) {
                IoBoardHl2::I2cTxn txn;
                txn.bus = producer;
                txn.address = quint8(i & 0x7F);
                txn.control = quint8((i >> 7) & 0xFF);
                txn.writeData = quint8((i >> 15) & 0xFF);
                if (io.enqueueI2c(txn)) {
                    ++i;
                } else {
                    std::this_thread::yield();
                }
            }
        };
        std::vector<int> next(2, 0);
        int received = 0;
        int outOfOrder = 0;
        int badDepth = 0;
        std::thread consumer([&]() {
            while (received < 2 * kPerProducer) {
                const int depth = io.i2cQueueDepth();
                if (depth < 0 || depth > IoBoardHl2::kMaxI2cQueue) {
                    ++badDepth;
                }
                IoBoardHl2::I2cTxn txn;
                if (!io.dequeueI2c(txn)) {
                    std::this_thread::yield();
                    continue;
                }
                const int producer = txn.bus;
                const int index = txn.address | (txn.control << 7) | (txn.writeData << 15);
                if (producer < 0 || producer > 1 || index != next[producer]) {
                    ++outOfOrder;
                } else {
                    ++next[producer];
                }
                ++received;
            }
        });
        std::thread a(produce, quint8(0));
        std::thread b(produce, quint8(1));
        a.join();
        b.join();
        consumer.join();
        QCOMPARE(outOfOrder, 0);
        QCOMPARE(badDepth, 0);
        QCOMPARE(received, 2 * kPerProducer);
        QCOMPARE(next[0], kPerProducer);
        QCOMPARE(next[1], kPerProducer);
        QVERIFY(io.i2cQueueIsEmpty());
    }
};

QTEST_APPLESS_MAIN(TestIoBoardHl2)
#include "tst_io_board_hl2.moc"
