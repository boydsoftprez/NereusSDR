// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_tx_channel_vox_listening.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TxChannel::setVoxListening() pump gate (Phase 3M-3a-iii
// Task 18 - bench fix).
//
// Source references (cited for traceability; logic lives in TxChannel.cpp):
//   wdsp/dexp.c:304 [v2.10.3.13]      - "DEXP code runs continuously so
//                                        it can be used to trigger VOX also."
//   audio.cs:168-192 [v2.10.3.13]     - Audio.VOXEnabled setter (no channel
//                                        state touch; only flips DEXP run_vox).
//   cmaster.cs:1039-1052 [v2.10.3.13] - cmaster.CMSetTXAVoxRun (sets DEXP
//                                        run_vox via SetDEXPRunVox).
//   TxChannel.cpp setVoxListening     - wraps the pump-gate state.
//
// Tests verify:
//   (a) Default vox-listening state is OFF (m_voxListening init false).
//   (b) setVoxListening(true) flips to ON; voxListeningForTest reads back true.
//   (c) setVoxListening(false) flips back to OFF; reads back false.
//   (d) Idempotent: setVoxListening(true)/setVoxListening(true) is a no-op.
//   (e) CRITICAL: setVoxListening operates independently of setRunning.
//       Both gates allow the pump independently; neither suppresses the
//       other.  The whole point of the bench fix.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-04 - New test file for Phase 3M-3a-iii Task 18 (bench fix):
//                 verifies the VOX-listening pump gate flag round-trips
//                 correctly and is independent of m_running.  J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#define NEREUS_BUILD_TESTS 1

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

class TstTxChannelVoxListening : public QObject {
    Q_OBJECT

    // 3M-3a-iii ships exactly one TxChannel - channel id 1 matches
    // RadioModel::connectToRadio() at WdspEngine::createTxChannel
    // (channelId=1).  See TxChannel.cpp s_voxKeyInstance comment block
    // for the phase 3F multi-pan TX expansion plan.
    static constexpr int kChannelId = 1;

private slots:

    // ---------------------------------------------------------------
    // (a) Default vox-listening state is OFF.
    //     m_voxListening initialises to false in TxChannel.h header.
    // ---------------------------------------------------------------
    void default_voxListening_isFalse() {
        TxChannel ch(kChannelId);
        QCOMPARE(ch.voxListeningForTest(), false);
    }

    // ---------------------------------------------------------------
    // (b) setVoxListening(true) flips to ON.
    // ---------------------------------------------------------------
    void setVoxListening_true_roundTrip() {
        TxChannel ch(kChannelId);
        ch.setVoxListening(true);
        QCOMPARE(ch.voxListeningForTest(), true);
    }

    // ---------------------------------------------------------------
    // (c) setVoxListening(false) after true flips back to OFF.
    // ---------------------------------------------------------------
    void setVoxListening_false_roundTrip() {
        TxChannel ch(kChannelId);
        ch.setVoxListening(true);
        ch.setVoxListening(false);
        QCOMPARE(ch.voxListeningForTest(), false);
    }

    // ---------------------------------------------------------------
    // (d) Idempotent guard: setVoxListening(true) twice in a row is
    //     a no-op (return after the wasOn==on check in setVoxListening).
    // ---------------------------------------------------------------
    void setVoxListening_idempotent() {
        TxChannel ch(kChannelId);
        ch.setVoxListening(true);
        ch.setVoxListening(true);  // no-op
        QCOMPARE(ch.voxListeningForTest(), true);

        ch.setVoxListening(false);
        ch.setVoxListening(false); // no-op
        QCOMPARE(ch.voxListeningForTest(), false);
    }

    // ---------------------------------------------------------------
    // (e) CRITICAL — independence from m_running.
    //
    // The whole point of the bench fix: m_running and m_voxListening
    // are independent gates.  Either alone is sufficient to allow the
    // pump (verified at the gate site in driveOneTxBlockFromInter
    // leaved); neither suppresses the other.  Without this property
    // the chicken-and-egg between MOX and DEXP returns.
    //
    // Test sequence mirrors the runtime VOX path:
    //   1. Default: both off (idle).
    //   2. User clicks [VOX]:  setVoxListening(true)  - listening only.
    //                         m_running stays false.
    //   3. User speaks, DEXP fires, MoxController engages MOX:
    //                         setRunning(true)        - both on.
    //   4. User stops, hang timer expires, MOX drops:
    //                         setRunning(false)       - listening only.
    //                         m_voxListening stays true (still armed for VOX).
    //   5. User clicks [VOX] off:
    //                         setVoxListening(false) - both off.
    // ---------------------------------------------------------------
    void setVoxListening_independentOfRunning() {
        TxChannel ch(kChannelId);

        // Step 1 — both off (idle).
        QCOMPARE(ch.isRunning(),            false);
        QCOMPARE(ch.voxListeningForTest(),  false);

        // Step 2 — user clicks [VOX]: listening on, MOX still off.
        ch.setVoxListening(true);
        QCOMPARE(ch.isRunning(),            false);  // setVoxListening MUST NOT touch m_running
        QCOMPARE(ch.voxListeningForTest(),  true);

        // Step 3 — DEXP fires, MoxController engages MOX: both on.
        ch.setRunning(true);
        QCOMPARE(ch.isRunning(),            true);
        QCOMPARE(ch.voxListeningForTest(),  true);   // unaffected by setRunning

        // Step 4 — VOX hang timer expires, MOX drops back: listening still on.
        ch.setRunning(false);
        QCOMPARE(ch.isRunning(),            false);
        QCOMPARE(ch.voxListeningForTest(),  true);   // still listening after MOX drop

        // Step 5 — user clicks [VOX] off: both off again.
        ch.setVoxListening(false);
        QCOMPARE(ch.isRunning(),            false);
        QCOMPARE(ch.voxListeningForTest(),  false);
    }
};

QTEST_APPLESS_MAIN(TstTxChannelVoxListening)
#include "tst_tx_channel_vox_listening.moc"
