// no-port-check: NereusSDR-original wiring test, no ported logic.
//
// tst_p1_frame_slot_after_removal — the compaction retunes the slot it moves.
//
// Codex review P1 on PR #311, "Preserve the Protocol 1 frame slot for a
// surviving stream", read as follows: remove Slice A while Slice B sits on
// stream 1, and deactivating receiver 0 compacts receiver 1 onto hardware
// slot 0; since the HL2 still announces two EP6 slots, Slice B would then
// read slot 0 / DDC0 instead of its own DDC1 and go silent or show the wrong
// band.
//
// The first half is right and the conclusion is not. rebuildHardwareMapping
// re-emits hardwareFrequencyChanged for every active receiver against its
// NEW hardware index, so the same call that moves the survivor down to slot 0
// also retunes DDC0 to the survivor's frequency
// (RadioModel::wireConnectionSignals forwards that to
// P1RadioConnection::setReceiverFrequency, which writes m_rxFreqHz[0]).
// Reading slot 0 is then correct, and DDC1 is left streaming a frequency
// nothing consumes.
//
// That correctness rests entirely on the re-emit, which nothing pinned. This
// test pins it: drop the re-emit loop from rebuildHardwareMapping and the
// compaction becomes exactly the defect Codex described.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/ReceiverManager.h"

using NereusSDR::ReceiverManager;

namespace {
constexpr quint64 kSliceAHz = 7'100'000;
constexpr quint64 kSliceBHz = 14'100'000;
} // namespace

class TstP1FrameSlotAfterRemoval : public QObject
{
    Q_OBJECT

private:
    // Protocol 1 leaves ddcIndex at -1 on purpose: publishDdcAssignment
    // excludes P1 from setDdcMapping because P1 packs the active receivers
    // sequentially into the EP6 frame and emits the frame-slot index, not the
    // DDC number. So rebuildHardwareMapping's sequential auto-assign is what
    // decides the slot, and this fixture reproduces that shape.
    static void twoActiveReceivers(ReceiverManager& rm)
    {
        rm.setMaxReceivers(2);
        QCOMPARE(rm.createReceiver(), 0);
        QCOMPARE(rm.createReceiver(), 1);
        rm.activateReceiver(0);
        rm.activateReceiver(1);
        rm.setReceiverFrequency(0, kSliceAHz);
        rm.setReceiverFrequency(1, kSliceBHz);
        QCOMPARE(rm.receiverConfig(0).hardwareRx, 0);
        QCOMPARE(rm.receiverConfig(1).hardwareRx, 1);
    }

private slots:
    // Codex's premise, confirmed: the survivor does move down a slot.
    void removingTheLowerStreamCompactsTheSurvivor()
    {
        ReceiverManager rm;
        twoActiveReceivers(rm);

        rm.deactivateReceiver(0);

        QCOMPARE(rm.receiverConfig(1).hardwareRx, 0);
        QCOMPARE(rm.activeReceiverCount(), 1);
    }

    // ...and the reason that is safe: the slot it moved onto is retuned to
    // the survivor's frequency in the same call. Without this the survivor
    // would inherit the removed slice's band, which is the reported defect.
    void theCompactionRetunesTheSlotItMovedOnto()
    {
        ReceiverManager rm;
        twoActiveReceivers(rm);

        QSignalSpy freqSpy(&rm, &ReceiverManager::hardwareFrequencyChanged);
        rm.deactivateReceiver(0);

        bool retunedSlotZero = false;
        for (const QList<QVariant>& e : freqSpy) {
            if (e.at(0).toInt() == 0 && e.at(1).toULongLong() == kSliceBHz) {
                retunedSlotZero = true;
            }
        }
        QVERIFY2(retunedSlotZero,
                 "frame slot 0 was handed to the survivor without being "
                 "retuned to its frequency");
    }

    // The routing table the I/Q path actually consults agrees: slot 0 is the
    // survivor's, and slot 1 no longer resolves to anyone.
    void theSurvivorReceivesOnItsNewSlotOnly()
    {
        ReceiverManager rm;
        twoActiveReceivers(rm);
        rm.deactivateReceiver(0);

        QSignalSpy fed(&rm, &ReceiverManager::iqDataForReceiver);
        rm.feedIqData(0, QVector<float>{1.0f, 0.0f});
        rm.feedIqData(1, QVector<float>{1.0f, 0.0f});

        QCOMPARE(fed.count(), 1);
        QCOMPARE(fed.at(0).at(0).toInt(), 1);
    }
};

QTEST_MAIN(TstP1FrameSlotAfterRemoval)
#include "tst_p1_frame_slot_after_removal.moc"
