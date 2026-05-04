// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_tx_channel_pushvox_callback.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TxChannel DEXP pushvox callback bridge (Phase 3M-3a-iii
// Task 17 — bench fix).
//
// Source references (cited for traceability; logic lives in TxChannel.cpp):
//   wdsp/dexp.c:330,339 [v2.10.3.13]   — pushvox firing points (LOW->ATTACK
//                                         transition + HOLD-timer expiry).
//   wdsp/dexp.c:399-403 [v2.10.3.13]   — SendCBPushDexpVox setter signature.
//   cmaster.cs:1125 [v2.10.3.13]       — Thetis-side analogue registration
//                                         (against ChannelMaster wrapper VOX,
//                                          not WDSP DEXP directly).
//   cmaster.cs:1903-1906 [v2.10.3.13]  — VOX.PushVox callback body.
//
// Tests verify:
//   (a) Callback for the registered channel id emits voxActiveChanged(true)
//       when active=1.
//   (b) Callback for the registered channel id emits voxActiveChanged(false)
//       when active=0.
//   (c) Callback for an unregistered / wrong channel id does NOT emit
//       (defensive guard for stale or cross-talk callbacks).
//
// Test seam: TxChannel::invokePushVoxForTest is exposed under
// NEREUS_BUILD_TESTS and forwards directly to the static
// s_pushVoxCallback bridge.  WDSP normally invokes the bridge from
// inside `xdexp` on the audio worker thread; tests cannot easily drive
// that path without a live mic stream + DEXP detector, so the seam
// invokes it synchronously.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-04 - New test file for Phase 3M-3a-iii Task 17 (bench fix):
//                 verifies the WDSP DEXP pushvox callback bridge correctly
//                 routes threshold-crossing events into the voxActiveChanged
//                 Qt signal (which RadioModel routes into MoxController::
//                 onVoxActive for direct MOX engagement).  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#define NEREUS_BUILD_TESTS 1

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/TxChannel.h"

using namespace NereusSDR;

class TstTxChannelPushVoxCallback : public QObject {
    Q_OBJECT

    // 3M-3a-iii ships exactly one TxChannel — channel id 1 matches
    // RadioModel::connectToRadio() at WdspEngine::createTxChannel
    // (channelId=1).  See TxChannel.cpp s_voxKeyInstance comment block
    // for the phase 3F multi-pan TX expansion plan (per-id table).
    static constexpr int kChannelId = 1;

private slots:

    // ---------------------------------------------------------------
    // Active=1 (mic envelope crossed attack threshold) -> emit true.
    // From Thetis wdsp/dexp.c:330 [v2.10.3.13]:
    //   (a->pushvox)(a->id, 1);
    // ---------------------------------------------------------------
    void s_pushVoxCallback_emitsTrueWhenActive() {
        TxChannel ch(kChannelId);
        QSignalSpy spy(&ch, &TxChannel::voxActiveChanged);
        QVERIFY(spy.isValid());

        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/1);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    // ---------------------------------------------------------------
    // Active=0 (HOLD timer expired) -> emit false.
    // From Thetis wdsp/dexp.c:339 [v2.10.3.13]:
    //   (a->pushvox)(a->id, 0);
    // ---------------------------------------------------------------
    void s_pushVoxCallback_emitsFalseWhenInactive() {
        TxChannel ch(kChannelId);
        QSignalSpy spy(&ch, &TxChannel::voxActiveChanged);
        QVERIFY(spy.isValid());

        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    // ---------------------------------------------------------------
    // Wrong channel id -> no emit.  Defensive against stale callbacks
    // fired during teardown (or cross-talk if phase 3F multi-pan TX
    // is added without expanding the s_voxKeyInstance table).
    // ---------------------------------------------------------------
    void s_pushVoxCallback_wrongChannelIdDoesNotEmit() {
        TxChannel ch(kChannelId);
        QSignalSpy spy(&ch, &TxChannel::voxActiveChanged);
        QVERIFY(spy.isValid());

        // Pass a channel id that does NOT match the registered instance.
        TxChannel::invokePushVoxForTest(kChannelId + 98, /*active=*/1);

        QCOMPARE(spy.count(), 0);
    }

    // ---------------------------------------------------------------
    // Multiple emits in sequence — verifies the bridge is re-entrant
    // for the same instance (no one-shot latch hidden in the bridge).
    // ---------------------------------------------------------------
    void s_pushVoxCallback_repeatedActivations() {
        TxChannel ch(kChannelId);
        QSignalSpy spy(&ch, &TxChannel::voxActiveChanged);
        QVERIFY(spy.isValid());

        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/1);  // attack
        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/0);  // release
        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/1);  // attack
        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/0);  // release

        QCOMPARE(spy.count(), 4);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
        QCOMPARE(spy.at(2).at(0).toBool(), true);
        QCOMPARE(spy.at(3).at(0).toBool(), false);
    }

    // ---------------------------------------------------------------
    // Post-destruction safety: a callback fired AFTER the TxChannel's
    // destructor has cleared s_voxKeyInstance must be a no-op (the
    // bridge's defensive null check).  This simulates the WDSP
    // worker-thread race where xdexp dispatches a callback that
    // hasn't yet executed when teardown runs.
    // ---------------------------------------------------------------
    void s_pushVoxCallback_afterDtorIsNoOp() {
        // Create + destroy a TxChannel; verify callback no-ops afterward.
        {
            TxChannel ch(kChannelId);
            // (signal spy intentionally NOT installed — ch goes out of
            // scope before invokePushVoxForTest fires.)
        }
        // ch destroyed here — s_voxKeyInstance is now nullptr.
        // No QSignalSpy receiver remains; the bridge must short-circuit
        // BEFORE the emit, or the test would crash on a deleted object.
        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/1);
        TxChannel::invokePushVoxForTest(kChannelId, /*active=*/0);
        // If we reach here without segfault, the dtor cleanup works.
        QVERIFY(true);
    }
};

QTEST_APPLESS_MAIN(TstTxChannelPushVoxCallback)
#include "tst_tx_channel_pushvox_callback.moc"
