// tst_receiver_manager_reconnect.cpp — issue #75 regression.
//
// Reproduces the same-session disconnect/reconnect bug that silently killed
// audio and spectrum on ANAN-7000DLE/8000DLE (OrionMkII) P2. Without
// ReceiverManager::reset() the second createReceiver() returns index 1,
// both receivers claim DDC2, and rebuildHardwareMapping() routes DDC2 I/Q
// to logical 1 whose wdspChannel is 1 — but only WDSP channel 0 exists, so
// the data path dead-ends.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/ReceiverManager.h"

using NereusSDR::ReceiverManager;
using NereusSDR::ReceiverConfig;

class TstReceiverManagerReconnect : public QObject
{
    Q_OBJECT

private slots:

    void firstConnectAssignsReceiver0ToDdc2()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        int rx = rm.createReceiver();
        QCOMPARE(rx, 0);

        rm.setDdcMapping(rx, 2);
        rm.activateReceiver(rx);

        const ReceiverConfig cfg = rm.receiverConfig(rx);
        QCOMPARE(cfg.hardwareRx, 2);
        QCOMPARE(cfg.wdspChannel, 0);
        QCOMPARE(rm.activeReceiverCount(), 1);
    }

    // Without reset(), a second "connect" sequence on the same manager
    // produces a receiver with index 1 and wdspChannel 1 — the exact
    // collision that breaks issue #75.
    void withoutResetSecondConnectLeaksReceiver()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        int rx1 = rm.createReceiver();
        rm.setDdcMapping(rx1, 2);
        rm.activateReceiver(rx1);

        // Simulate reconnect without reset — what the old teardown did.
        int rx2 = rm.createReceiver();
        QCOMPARE(rx2, 1);                  // leaked counter hands out 1

        rm.setDdcMapping(rx2, 2);
        rm.activateReceiver(rx2);

        // Both receivers now claim DDC2. Last writer wins in rebuildHardwareMapping
        // (QMap iterates ascending by key, so rx2 overwrites rx1's slot).
        const ReceiverConfig cfg2 = rm.receiverConfig(rx2);
        QCOMPARE(cfg2.wdspChannel, 1);     // but only WDSP channel 0 exists
        QCOMPARE(rm.activeReceiverCount(), 2);
    }

    // With reset(), the next connect sees a clean manager and gets
    // receiver 0 / wdspChannel 0 back — matches the first-connect state.
    void resetRestoresFreshConnectState()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        int rx1 = rm.createReceiver();
        rm.setDdcMapping(rx1, 2);
        rm.activateReceiver(rx1);

        QSignalSpy destroyedSpy(&rm, &ReceiverManager::receiverDestroyed);
        QSignalSpy countSpy(&rm, &ReceiverManager::activeReceiverCountChanged);

        rm.reset();

        QCOMPARE(rm.activeReceiverCount(), 0);
        QCOMPARE(destroyedSpy.count(), 1);
        QCOMPARE(destroyedSpy.takeFirst().at(0).toInt(), 0);
        QVERIFY(countSpy.count() >= 1);
        QCOMPARE(countSpy.last().at(0).toInt(), 0);

        int rx2 = rm.createReceiver();
        QCOMPARE(rx2, 0);                  // counter reset

        rm.setDdcMapping(rx2, 2);
        rm.activateReceiver(rx2);

        const ReceiverConfig cfg = rm.receiverConfig(rx2);
        QCOMPARE(cfg.hardwareRx, 2);
        QCOMPARE(cfg.wdspChannel, 0);      // matches WDSP channel 0
        QCOMPARE(rm.activeReceiverCount(), 1);
    }

    // I/Q fed through a just-reset manager must land on receiver 0, not
    // leak to a stale mapping. This is the end-to-end behavior issue #75
    // reporters actually see.
    void resetRoutesSubsequentIqToReceiver0()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        int rx1 = rm.createReceiver();
        rm.setDdcMapping(rx1, 2);
        rm.activateReceiver(rx1);

        rm.reset();

        int rx2 = rm.createReceiver();
        rm.setDdcMapping(rx2, 2);
        rm.activateReceiver(rx2);

        QSignalSpy forwarded(&rm, &ReceiverManager::iqDataForReceiver);

        QVector<float> samples{1.0f, 0.0f, 0.5f, 0.5f};
        rm.feedIqData(2, samples);

        QCOMPARE(forwarded.count(), 1);
        QCOMPARE(forwarded.first().at(0).toInt(), 0);   // logical 0, not 1
    }

    // ── Phase 3F Sub-Epic I closeout, defect C2 ───────────────────────────
    //
    // setDdcMapping had no change gate. Before Sub-Epic I it ran once per
    // connect; RadioModel::publishDdcAssignment now calls it for every
    // active stream on every requestDdcAssignment, which fires on every
    // slice frequencyChanged. Each redundant call rebuilt the hardware
    // mapping, and rebuildHardwareMapping re-emits every active receiver's
    // STORED frequency — so a spun encoder produced a stream of duplicate
    // P2 command frames, and under CTUN it retuned the DDC back to the
    // pre-drag centre, yanking the pan out from under the operator.

    void redundantDdcMappingDoesNotRetuneTheDdc()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        const int rx = rm.createReceiver();
        rm.setDdcMapping(rx, 2);
        rm.activateReceiver(rx);
        rm.setReceiverFrequency(rx, 14200000);

        // Spy AFTER setup so only the redundant republish is observed.
        QSignalSpy hwFreq(&rm, &ReceiverManager::hardwareFrequencyChanged);

        // publishDdcAssignment pushing the same mapping on a VFO tick.
        rm.setDdcMapping(rx, 2);
        rm.setDdcMapping(rx, 2);
        rm.setDdcMapping(rx, 2);

        QCOMPARE(hwFreq.count(), 0);
    }

    // A real mapping change must still rebuild and re-emit.
    void changedDdcMappingStillRebuilds()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        const int rx = rm.createReceiver();
        rm.setDdcMapping(rx, 2);
        rm.activateReceiver(rx);
        rm.setReceiverFrequency(rx, 14200000);

        QSignalSpy hwFreq(&rm, &ReceiverManager::hardwareFrequencyChanged);

        rm.setDdcMapping(rx, 3);

        QCOMPARE(rm.receiverConfig(rx).hardwareRx, 3);
        QVERIFY(hwFreq.count() >= 1);
        QCOMPARE(hwFreq.last().at(0).toInt(), 3);
        QCOMPARE(hwFreq.last().at(1).toULongLong(), quint64(14200000));
    }

    // The CTUN pan drag goes through forceHardwareFrequency, which bypasses
    // the DDC lock. It now stores the frequency too, so a rebuild triggered
    // later (a genuine mapping change, a receiver activation) re-emits the
    // dragged-to centre rather than the stale pre-drag one.
    void forcedFrequencySurvivesALaterRebuild()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        const int rx = rm.createReceiver();
        rm.setDdcMapping(rx, 2);
        rm.activateReceiver(rx);
        rm.setReceiverFrequency(rx, 14200000);

        // CTUN on: a VFO move inside the pinned window must not retune.
        rm.setDdcFrequencyLocked(true);

        // Operator drags the pan.
        rm.forceHardwareFrequency(rx, 14250000);

        QSignalSpy hwFreq(&rm, &ReceiverManager::hardwareFrequencyChanged);

        // Anything that rebuilds must re-assert the dragged-to centre.
        rm.setDdcMapping(rx, 3);

        QVERIFY(hwFreq.count() >= 1);
        QCOMPARE(hwFreq.last().at(1).toULongLong(), quint64(14250000));
    }

    // forceHardwareFrequency must not start announcing a logical retune:
    // the DDC moved, the receiver's tuning did not.
    void forcedFrequencyDoesNotEmitReceiverFrequencyChanged()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        const int rx = rm.createReceiver();
        rm.setDdcMapping(rx, 2);
        rm.activateReceiver(rx);

        QSignalSpy logical(&rm, &ReceiverManager::receiverFrequencyChanged);
        rm.forceHardwareFrequency(rx, 14250000);

        QCOMPARE(logical.count(), 0);
    }

    // P1 path is unaffected by the bug — auto hw assignment avoids
    // collision even without reset — but reset() must not break it.
    void resetPreservesAutoAssignForP1()
    {
        ReceiverManager rm;
        rm.setMaxReceivers(2);

        int rx1 = rm.createReceiver();   // no setDdcMapping — P1 path
        rm.activateReceiver(rx1);
        QCOMPARE(rm.receiverConfig(rx1).hardwareRx, 0);

        rm.reset();

        int rx2 = rm.createReceiver();
        rm.activateReceiver(rx2);
        QCOMPARE(rx2, 0);
        QCOMPARE(rm.receiverConfig(rx2).hardwareRx, 0);
    }
};

QTEST_MAIN(TstReceiverManagerReconnect)
#include "tst_receiver_manager_reconnect.moc"
