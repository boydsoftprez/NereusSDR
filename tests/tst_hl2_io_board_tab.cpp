// no-port-check: smoke test for HL2 I/O Setup page UI construction + signal binding
#include <QtTest/QtTest>
#include <QApplication>

#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "models/RadioModel.h"
#include "core/IoBoardHl2.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestHl2IoBoardTab : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    // Construction succeeds for HL2 board (tab visible)
    void construct_hl2_board_visible() {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);
        // Smoke: widget constructed without crash
        QVERIFY(true);
    }

    // Construction succeeds for non-HL2 board (widget auto-hidden)
    void construct_hermes_board_hidden() {
        RadioModel model;
        // Default board (HERMES) — tab should be hidden
        Hl2IoBoardTab tab(&model);
        // Smoke: construction without crash; visibility is gated by board model
        QVERIFY(true);
    }

    // Register change in model propagates to (and does not crash) the UI
    void register_change_updates_ui() {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);
        model.ioBoardMutable().setRegisterValue(
            IoBoardHl2::Register::HardwareVersion, 0x03);
        // Verify the model accepted the value (signal-driven UI update should not crash)
        QCOMPARE(int(model.ioBoard().registerValue(IoBoardHl2::Register::HardwareVersion)),
                 0x03);
    }

    // Step advance propagates to UI without crash
    void step_advance_updates_ui() {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);
        for (int i = 0; i < IoBoardHl2::kStateMachineSteps + 2; ++i) {
            model.ioBoardMutable().advanceStep();
        }
        QVERIFY(true);  // smoke: no crash through full cycle + wrap
    }

    // Detection signal propagates to UI without crash
    void detection_signal_propagates() {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);
        model.ioBoardMutable().setDetected(true);
        QVERIFY(model.ioBoard().isDetected());
        model.ioBoardMutable().setDetected(false);
        QVERIFY(!model.ioBoard().isDetected());
    }

    // Throttle change in monitor propagates to UI without crash.
    // Uses real-clock ticks (QTest::qSleep) to satisfy compute_bps time delta.
    void throttle_change_updates_ui() {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        // Establish baseline so compute_bps sees real time deltas.
        model.bwMonitorMutable().recordEp6Bytes(10000);
        model.bwMonitorMutable().recordEp2Bytes(5000);
        model.bwMonitorMutable().tick();

        // Feed ep2 to establish rate
        for (int i = 0; i < 3; ++i) {
            QTest::qSleep(5);
            model.bwMonitorMutable().recordEp6Bytes(10000);
            model.bwMonitorMutable().recordEp2Bytes(5000);
            model.bwMonitorMutable().tick();
        }

        // Now let ep6 go silent while ep2 stays active → throttle after N ticks
        for (int i = 0; i < HermesLiteBandwidthMonitor::kThrottleTickThreshold; ++i) {
            QTest::qSleep(5);
            model.bwMonitorMutable().recordEp2Bytes(5000);
            model.bwMonitorMutable().tick();
        }
        QVERIFY(model.bwMonitor().isThrottled());
    }

    // I2C queue change propagates to UI without crash
    void i2c_queue_change_propagates() {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        IoBoardHl2::I2cTxn txn{};
        txn.bus     = IoBoardHl2::kI2cBusIndex;
        txn.address = IoBoardHl2::kI2cAddrGeneral;
        txn.control = IoBoardHl2::CtrlWrite;
        txn.writeData = 0xAB;

        model.ioBoardMutable().enqueueI2c(txn);
        QCOMPARE(model.ioBoard().i2cQueueDepth(), 1);

        IoBoardHl2::I2cTxn out;
        model.ioBoardMutable().dequeueI2c(out);
        QCOMPARE(model.ioBoard().i2cQueueDepth(), 0);
    }
};

QTEST_MAIN(TestHl2IoBoardTab)
#include "tst_hl2_io_board_tab.moc"
