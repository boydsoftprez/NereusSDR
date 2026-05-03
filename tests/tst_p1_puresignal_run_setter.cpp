// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setPuresignalRun()
// (Task 2.3 of the P1 full-parity epic).
//
// puresignal_run bit position: bank 11 (C0=0x14) C2 byte, bit 6 (mask 0x40).
// Boolean field — the codec emits `(ctx.p1PuresignalRun ? 0x40 : 0x00)`,
// stored verbatim in the shared base m_puresignalRun.
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
//     case 11:
//       C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
//     → puresignal_run occupies bit 6 of C2 in bank 11; the low 5 bits
//       carry line_in_gain (Task 2.1).  Bit 5 is unused; bit 7 is unused.
//
// Bank 11 byte layout for C0=0x14:
//   out[0] = 0x14 (C0 — address ORed with MOX bit 0)
//   out[1] = preamp bits + mic_trs/bias/ptt
//   out[2] = line_in_gain (low 5 bits) + puresignal_run (bit 6)
//   out[3] = user digital outputs (low 4 bits)
//   out[4] = ADC0 RX step ATT (5-bit + 0x20 enable)
//
// Test seam: captureBank11ForTest() calls composeCcForBankForTest(11, ...)
// which routes through composeCcForBank().  The codec path (when
// setBoardForTest() is called) reads ctx.p1PuresignalRun and sets bit 6.
// The legacy compose path emits 0 for C2 unconditionally — this is
// pre-existing behaviour predating Task 1.2; tests here therefore use
// setBoardForTest() to exercise the codec path.
//
// Semantic note: this flag is the *runtime* state of "PureSignal feedback
// DDC routing currently active", distinct from BoardCapabilities.hasPureSignal
// (capability) and from TransmitModel::puresignalEnabled (user toggle).
// Until 3M-4 lights up the actual feedback DDC routing, the
// PureSignalApplet "Enable" toggle drives this through Task 2.5 wiring.

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1PuresignalRunSetter : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: puresignal_run bit 6 is CLEAR ──────────────────────
    // m_puresignalRun defaults false, so C2 bit 6 must be 0 (mask 0x40).
    // Source: Thetis networkproto1.c:600 [v2.10.3.13]
    //   ((prn->puresignal_run & 1) << 6); → 0 when puresignal_run = false.
    void defaultState_puresignalRunBitIsClear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard (codec path)

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C2 = bank11[2], bit 6 (0x40) must be 0 by default.
        QCOMPARE(int(quint8(bank11[2]) & 0x40), 0);
    }

    // ── 2. setPuresignalRun(false) → C2 bit 6 = 0 ────────────────────────────
    // Explicit false call (mirrors Task 2.2 zero-value baseline).
    // Source: Thetis networkproto1.c:600 [v2.10.3.13]
    void setPuresignalRunFalse_clearsBit6() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setPuresignalRun(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[2]) & 0x40), 0);
    }

    // ── 3. setPuresignalRun(true) → C2 bit 6 = 0x40 ──────────────────────────
    // True path — bit 6 must be set, no other bits propagated.
    // Source: Thetis networkproto1.c:600 [v2.10.3.13]
    void setPuresignalRunTrue_setsBit6() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setPuresignalRun(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 6 (mask 0x40) must be set.
        QCOMPARE(int(quint8(bank11[2]) & 0x40), 0x40);
    }

    // ── 4. setPuresignalRun(true) does NOT touch C2 low 5 bits (line_in_gain) ─
    // Cross-bit guard.  C2 low 5 bits carry line_in_gain (Task 2.1) — must
    // remain 0 when only setPuresignalRun is called and m_lineInGain default 0.
    // Source: Thetis networkproto1.c:600 [v2.10.3.13]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    void setPuresignalRun_doesNotTouchLineInGainBits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setPuresignalRun(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C2 low 5 bits (mask 0x1F) must remain 0 (line_in_gain default 0).
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 0);
        // Whole C2 byte must equal exactly 0x40 (only bit 6 set).
        QCOMPARE(int(quint8(bank11[2])), 0x40);
    }

    // ── 5. setPuresignalRun does NOT touch C1 (mic bits) ─────────────────────
    // Cross-bit guard.  C1 carries preamp + mic_trs (bit 4) + mic_bias (bit 5)
    // + mic_ptt (bit 6) — NONE of which puresignal_run should touch.
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13]
    void setPuresignalRun_doesNotTouchC1MicBits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        // Defaults: m_micBias=false, m_micPTT=false, m_micTipRing=true.
        // mic_trs is INVERTED on the wire so default !true = 0 in bit 4.
        // → C1 should be entirely 0.
        conn.setPuresignalRun(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C1 = bank11[1] must remain 0 (no mic-jack defaults flipped).
        QCOMPARE(int(quint8(bank11[1])), 0);
    }

    // ── 6. setPuresignalRun does NOT touch C3 (user_dig_out) ─────────────────
    // Cross-bit guard.  C3 carries user_dig_out (low 4 bits, Task 2.2) — must
    // remain 0 when only setPuresignalRun is called and m_userDigOut default 0.
    // Source: Thetis networkproto1.c:601 [v2.10.3.13]
    //   C3 = prn->user_dig_out & 0b00001111;
    void setPuresignalRun_doesNotTouchC3UserDigOut() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setPuresignalRun(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C3 = bank11[3] must remain 0 (user_dig_out default 0).
        QCOMPARE(int(quint8(bank11[3])), 0);
    }

    // ── 7. Bank 11 C0 address must remain 0x14 ───────────────────────────────
    // Source: Thetis networkproto1.c:594 [v2.10.3.13] — case 11 sets C0 |= 0x14.
    void setPuresignalRun_c0AddressIs0x14() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setPuresignalRun(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C0 = bank11[0]. Bits 7..1 = address 0x14; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x14.
        QCOMPARE(int(quint8(bank11[0]) & 0xFE), 0x14);
    }

    // ── 8. Round-trip: false → true → false ──────────────────────────────────
    // Each transition lands bit-correct.
    void roundTrip_falseTrueFalse_bitTracks() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setPuresignalRun(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0);

        conn.setPuresignalRun(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0x40);

        conn.setPuresignalRun(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0);
    }

    // ── 9. Flush flag: setPuresignalRun sets m_forceBank11Next (Codex P2) ────
    // Reuses m_forceBank11Next from G.3 (same bank 11 byte family).  Flush
    // flag must be set BEFORE the idempotent guard (Codex P2 pattern).
    void setPuresignalRun_setsForceBank11Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank11NextForTest(), false);

        conn.setPuresignalRun(true);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 10. Flush flag set even when value doesn't change (Codex P2) ─────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // (flush flag) must still fire.
    void setPuresignalRunRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setPuresignalRun(false);  // no state change (default is false)
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 11. Idempotent: setPuresignalRun(true) twice doesn't crash ───────────
    void setPuresignalRunTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setPuresignalRun(true);
        conn.setPuresignalRun(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[2]) & 0x40), 0x40);
    }

    // ── 12. Codec path (HL2): setPuresignalRun stores but C2 unchanged ───────
    // HL2 firmware has no on-radio PureSignal feedback path and the
    // P1CodecHl2 bank-11 case emits out[2] = 0 unconditionally (see
    // P1CodecHl2.cpp:238) — puresignal_run is a non-feature on HL2.  This
    // test pins the cross-codec contract: the setter still stores the flag
    // (and the bank-11 flush flag fires) but the HL2 codec deliberately
    // does not propagate it.  Documents the intentional codec divergence
    // and prevents future regressions if someone extends the HL2 codec to
    // honour the field.
    void setPuresignalRun_hl2CodecPath_c2RemainsZero() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setPuresignalRun(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // HL2 codec emits out[2] = 0 regardless of ctx.p1PuresignalRun.
        QCOMPARE(int(quint8(bank11[2])), 0);
        // Flush flag still fires — Codex P2 safety contract is independent
        // of the codec's emission decision.
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }
};

QTEST_APPLESS_MAIN(TestP1PuresignalRunSetter)
#include "tst_p1_puresignal_run_setter.moc"
