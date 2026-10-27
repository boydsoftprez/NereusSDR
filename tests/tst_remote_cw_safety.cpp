// no-port-check: NereusSDR-original behavioral tests.
// =================================================================
// tests/tst_remote_cw_safety.cpp (NereusSDR)
// =================================================================
//
// Remote CW hold: one activity epoch owns its independent hold reasons.
// Cancellation prevents recovery until matching remote intent is cleared;
// a physical observation invalidates only during a network hold.
// These tests exercise the value directly, without a radio, sender, clock,
// transport or application wiring. The guard-loop stall test is Task 4.
//
// Modification history (NereusSDR):
//   2026-10-27: Prepare the pure safety-value behavioral RED cases for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#include <QtTest/QtTest>

#include <barrier>
#include <thread>

#include "core/safety/RemoteCwSafetyState.h"

using namespace NereusSDR;

class TestRemoteCwSafetyState : public QObject {
    Q_OBJECT

private slots:
    // With no live activity, stale callbacks cannot create or cancel one.
    void noActivityRejectsAllMutations()
    {
        RemoteCwSafetyState state;
        QVERIFY(!state.setHold(11, TxChannel::RfHold::Link));
        QVERIFY(!state.clearHold(11, TxChannel::RfHold::Link));
        QVERIFY(!state.invalidate(11));
        QVERIFY(!state.invalidatePhysicalPress(11));
        state.finish(11);
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{0});
        QCOMPARE(after.holdMask, quint32{0});
        QVERIFY(!after.valid);
        QVERIFY(!after.invalidated);
    }

    // Zero is the absent-activity sentinel, never a usable lease.
    void zeroEpochCannotCreateActivity()
    {
        RemoteCwSafetyState state;
        state.begin(0);
        QVERIFY(!state.setHold(0, TxChannel::RfHold::Guard));
        QVERIFY(!state.clearHold(0, TxChannel::RfHold::Guard));
        QVERIFY(!state.invalidate(0));
        QVERIFY(!state.invalidatePhysicalPress(0));
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{0});
        QCOMPARE(after.holdMask, quint32{0});
        QVERIFY(!after.valid);
        QVERIFY(!after.invalidated);
    }

    // Catches a missing admission transition and an always-suppress gate.
    void beginRecordsLiveUnheldEpoch()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{11});
        QCOMPARE(after.holdMask, quint32{0});
        QVERIFY(after.valid);
        QVERIFY(!after.invalidated);
        QVERIFY(!state.suppressesCw());
    }

    void independentReasons_data()
    {
        QTest::addColumn<int>("reasonToClear");
        QTest::addColumn<quint32>("remainingMask");
        // TxChannel::RfHold defines Link=1, Mic=2, Guard=4. The literal
        // remaining masks pin independent reason composition, not packing.
        QTest::newRow("clear-link") << int(TxChannel::RfHold::Link) << quint32{6};
        QTest::newRow("clear-mic") << int(TxChannel::RfHold::Mic) << quint32{5};
        QTest::newRow("clear-guard") << int(TxChannel::RfHold::Guard) << quint32{3};
    }

    // Clearing one source must never release another source's hold.
    void independentReasons()
    {
        QFETCH(int, reasonToClear);
        QFETCH(quint32, remainingMask);
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.setHold(11, TxChannel::RfHold::Link));
        QVERIFY(state.setHold(11, TxChannel::RfHold::Mic));
        QVERIFY(state.setHold(11, TxChannel::RfHold::Guard));
        QCOMPARE(state.snapshot().holdMask, quint32{7});
        QVERIFY(state.suppressesCw());
        QVERIFY(state.clearHold(11, static_cast<TxChannel::RfHold>(reasonToClear)));
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{11});
        QCOMPARE(after.holdMask, remainingMask);
        QVERIFY(after.valid);
        QVERIFY(!after.invalidated);
        QVERIFY(state.suppressesCw());
    }

    // A delayed old callback cannot mutate the current epoch's mask.
    void oldEpochCannotHoldOrRelease()
    {
        RemoteCwSafetyState state;
        state.begin(12);
        QVERIFY(state.setHold(12, TxChannel::RfHold::Guard));
        QVERIFY(!state.setHold(11, TxChannel::RfHold::Link));
        QVERIFY(!state.clearHold(11, TxChannel::RfHold::Guard));
        QVERIFY(!state.invalidate(11));
        QVERIFY(!state.invalidatePhysicalPress(11));
        state.finish(11);
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{12});
        QCOMPARE(after.holdMask, quint32{4});
        QVERIFY(after.valid);
        QVERIFY(!after.invalidated);
        QVERIFY(state.suppressesCw());
    }

    // A canceled checkpoint cannot recover merely because liveness returns.
    void invalidationBlocksRecoveryUntilFinish()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.setHold(11, TxChannel::RfHold::Link));
        QVERIFY(state.invalidate(11));
        QVERIFY(!state.clearHold(11, TxChannel::RfHold::Link));
        QVERIFY(!state.setHold(11, TxChannel::RfHold::Guard));
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{11});
        QCOMPARE(after.holdMask, quint32{1});
        QVERIFY(!after.valid);
        QVERIFY(after.invalidated);
        QVERIFY(state.suppressesCw());
    }

    void physicalPressOnlyInvalidatesNetworkHold_data()
    {
        QTest::addColumn<int>("holdMask");
        QTest::addColumn<bool>("invalidates");
        QTest::newRow("no-hold") << 0 << false;
        QTest::newRow("mic-only") << 2 << false;
        QTest::newRow("link") << 1 << true;
        QTest::newRow("guard") << 4 << true;
        QTest::newRow("mic-and-link") << 3 << true;
    }

    // Catches broadening the approved network-hold takeover to ordinary
    // operation or a mic-only hold. The caller owns physical edge detection.
    void physicalPressOnlyInvalidatesNetworkHold()
    {
        QFETCH(int, holdMask);
        QFETCH(bool, invalidates);
        RemoteCwSafetyState state;
        state.begin(11);
        QCOMPARE(state.snapshot().epoch, quint32{11});
        if ((holdMask & 1) != 0) {
            QVERIFY(state.setHold(11, TxChannel::RfHold::Link));
        }
        if ((holdMask & 2) != 0) {
            QVERIFY(state.setHold(11, TxChannel::RfHold::Mic));
        }
        if ((holdMask & 4) != 0) {
            QVERIFY(state.setHold(11, TxChannel::RfHold::Guard));
        }
        QCOMPARE(state.invalidatePhysicalPress(11), invalidates);
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{11});
        QCOMPARE(after.holdMask, quint32(holdMask));
        QCOMPARE(after.valid, !invalidates);
        QCOMPARE(after.invalidated, invalidates);
    }

    // Observation wins the same state transition that recovery must clear.
    void physicalObservationBeforeRecoveryPreventsRelease()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.setHold(11, TxChannel::RfHold::Link));
        QVERIFY(state.invalidatePhysicalPress(11));
        QVERIFY(!state.clearHold(11, TxChannel::RfHold::Link));
        QVERIFY(state.snapshot().invalidated);
        QVERIFY(!state.snapshot().valid);
        QVERIFY(state.suppressesCw());
    }

    // Recovery wins first: the next observation is outside network hold.
    void recoveryBeforePhysicalObservationAddsNoTakeover()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.setHold(11, TxChannel::RfHold::Link));
        QVERIFY(state.clearHold(11, TxChannel::RfHold::Link));
        QVERIFY(!state.invalidatePhysicalPress(11));
        const RemoteCwSafetySnapshot after = state.snapshot();
        QVERIFY(after.valid);
        QVERIFY(!after.invalidated);
        QCOMPARE(after.holdMask, quint32{0});
        QVERIFY(!state.suppressesCw());
    }

    // One recovered path cannot remove the other path's takeover interval.
    void remainingNetworkReasonStillAllowsTakeover()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.setHold(11, TxChannel::RfHold::Link));
        QVERIFY(state.setHold(11, TxChannel::RfHold::Guard));
        QVERIFY(state.clearHold(11, TxChannel::RfHold::Link));
        QVERIFY(state.invalidatePhysicalPress(11));
        QVERIFY(!state.clearHold(11, TxChannel::RfHold::Guard));
        QCOMPARE(state.snapshot().holdMask, quint32{4});
        QVERIFY(state.suppressesCw());
    }

    // A completion queued for a finished activity cannot clear its successor.
    void finishedOldEpochCannotClearNew()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.invalidate(11));
        state.finish(11); // Caller has already cleared epoch 11's intent.
        state.begin(12);
        QVERIFY(state.setHold(12, TxChannel::RfHold::Guard));
        state.finish(11);
        QVERIFY(!state.clearHold(11, TxChannel::RfHold::Guard));
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{12});
        QCOMPARE(after.holdMask, quint32{4});
        QVERIFY(after.valid);
        QVERIFY(state.suppressesCw());
    }

    // A canceled remote epoch remains suppressed even without a hold bit;
    // matching finish releases it only after the caller cleared its intent.
    void currentFinishClearsInvalidatedEpoch()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.invalidate(11));
        QVERIFY(state.suppressesCw());
        state.finish(11);
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{0});
        QCOMPARE(after.holdMask, quint32{0});
        QVERIFY(!after.valid);
        QVERIFY(!after.invalidated);
        QVERIFY(!state.suppressesCw());
    }

    // Catches a check-then-clear race that reports both operations accepted.
    // The two sequential tests above pin each legal ordering independently.
    void concurrentPhysicalAndRecoveryHaveOnlyLegalOutcomes()
    {
        RemoteCwSafetyState state;
        state.begin(11);
        QVERIFY(state.setHold(11, TxChannel::RfHold::Link));
        std::barrier ready(3);
        bool released = false;
        bool invalidated = false;
        std::thread recovery([&]() {
            ready.arrive_and_wait();
            released = state.clearHold(11, TxChannel::RfHold::Link);
        });
        std::thread observation([&]() {
            ready.arrive_and_wait();
            invalidated = state.invalidatePhysicalPress(11);
        });
        ready.arrive_and_wait();
        recovery.join();
        observation.join();
        QVERIFY(released != invalidated);
        const RemoteCwSafetySnapshot after = state.snapshot();
        QCOMPARE(after.epoch, quint32{11});
        QCOMPARE(after.valid, released);
        QCOMPARE(after.invalidated, invalidated);
        QCOMPARE(after.holdMask, invalidated ? quint32{1} : quint32{0});
        QCOMPARE(state.suppressesCw(), invalidated);
    }
};

QTEST_GUILESS_MAIN(TestRemoteCwSafetyState)
#include "tst_remote_cw_safety.moc"
