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
