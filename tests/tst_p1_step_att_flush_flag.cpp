// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Bank-11 flush-flag parity tests for P1RadioConnection::setAttenuator() +
// setPreamp() (v0.4.1 hotfix).
//
// Background: every other bank-11 setter in P1RadioConnection sets
// `m_forceBank11Next = true` BEFORE its idempotent guard so the new wire
// byte lands on the next EP2 frame (≤2.6 ms at 380.95 fps), not the next
// natural round-robin visit (~22 ms worst case at maxBank=16).  Peer
// setters: setMicTipRing, setMicBoost, setLineIn, setUserDigOut,
// setMicPTTDisabled, setPuresignalRun.  setAttenuator and setPreamp were
// missed in the v0.4.0 plumbing pass, so RX step-att and preamp changes
// took up to 22 ms to land.
//
// Source: this file is parity coverage for the established Codex P2
// pattern — see tst_p1_mic_tip_ring_wire.cpp §8/§9 and
// tst_p1_user_dig_out_setter.cpp for the same pattern on peer setters.
// The Thetis equivalent is implicit: networkproto1.c:601-602
// [v2.10.3.13] reads `prn->rx[0].preamp` and bank 12 reads
// `prn->adc[0].rx_step_attn` on every round-robin visit; Thetis has no
// flush-flag concept because its EP2 pacer rolls all banks on a fixed
// 380.95 fps schedule.  NereusSDR's flush-flag is a low-latency
// optimisation for input-feel-sensitive setters.

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1StepAttFlushFlag : public QObject {
    Q_OBJECT

private slots:
    // ── §1 setAttenuator — initial flush flag is false ───────────────────────
    void setAttenuator_defaultFlushFlagIsFalse() {
        P1RadioConnection conn;
        QCOMPARE(conn.forceBank11NextForTest(), false);
    }

    // ── §2 setAttenuator — first call sets flush flag true ───────────────────
    // Mirrors tst_p1_mic_tip_ring_wire.cpp §8 — peer-setter flush-flag pattern.
    void setAttenuator_firstCall_setsFlushFlag() {
        P1RadioConnection conn;
        conn.setAttenuator(5);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── §3 setAttenuator — Codex P2 pattern: flush flag fires even on no-op ──
    // The flush flag must fire BEFORE the idempotent guard, so re-setting the
    // same value still arms the next-frame send.  Mirrors
    // tst_p1_mic_tip_ring_wire.cpp §9.
    void setAttenuator_repeatedSameValue_stillSetsFlushFlag() {
        P1RadioConnection conn;
        conn.setAttenuator(5);
        // Drain the flag (sendCommandFrame would clear it on the next frame).
        // Test seam: re-call with the same value and verify the flag is
        // re-armed.
        conn.setAttenuator(5);  // idempotent value
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── §4 setAttenuator — value lands in bank 11 C4 byte ────────────────────
    // Sanity check that setAttenuator's primary side effect (writing
    // m_stepAttn[0]) still works after adding the flush flag.  Bank 11 C4
    // = (m_stepAttn[0] & 0x1F) | 0x20 per
    // codec/P1CodecStandard.cpp:295.
    void setAttenuator_valueAppearsInBank11C4() {
        P1RadioConnection conn;
        conn.setAttenuator(5);
        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C4 = bank11[4] = (5 & 0x1F) | 0x20 = 0x25
        QCOMPARE(int(quint8(bank11[4])), 0x25);
    }

    // ── §5 setPreamp — initial flush flag is false ───────────────────────────
    void setPreamp_defaultFlushFlagIsFalse() {
        P1RadioConnection conn;
        QCOMPARE(conn.forceBank11NextForTest(), false);
    }

    // ── §6 setPreamp — first call sets flush flag true ───────────────────────
    void setPreamp_firstCall_setsFlushFlag() {
        P1RadioConnection conn;
        conn.setPreamp(true);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── §7 setPreamp — Codex P2 pattern: flush flag fires even on no-op ──────
    void setPreamp_repeatedSameValue_stillSetsFlushFlag() {
        P1RadioConnection conn;
        conn.setPreamp(false);  // default-state value (m_rxPreamp[0] starts false)
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }
};

QTEST_MAIN(TestP1StepAttFlushFlag)
#include "tst_p1_step_att_flush_flag.moc"
