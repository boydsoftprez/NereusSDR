// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setUserDigOut()
// (Task 2.2 of the P1 full-parity epic).
//
// user_dig_out bit positions: bank 11 (C0=0x14) C3 byte, low 4 bits
// (mask 0x0F).  Values are masked to the low nibble at the API boundary
// via & 0x0F so callers cannot accidentally smuggle high-nibble bits onto
// the wire; the codec also masks via `(ctx.p1UserDigOut & 0x0F)`, but the
// API-side mask keeps stored state matching wire bytes 1:1.
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13]
//     case 11:
//       C3 = prn->user_dig_out & 0b00001111;
//     → user_dig_out occupies low 4 bits of C3 in bank 11.
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
// setBoardForTest() is called) reads ctx.p1UserDigOut and emits the low 4
// bits.  The legacy compose path emits 0 for C3 unconditionally — this is
// pre-existing behaviour predating Task 2.2; tests here therefore use
// setBoardForTest() to exercise the codec path.

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1UserDigOutSetter : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: user_dig_out low 4 bits are CLEAR ──────────────────
    // m_userDigOut defaults 0, so C3 low 4 bits must be 0.
    // Source: Thetis networkproto1.c:601 [v2.10.3.13]
    //   C3 = prn->user_dig_out & 0b00001111; → 0 when user_dig_out = 0.
    void defaultState_userDigOutBitsAreClear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard (codec path)

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C3 = bank11[3], low 4 bits (0x0F) must be 0 by default.
        QCOMPARE(int(quint8(bank11[3]) & 0x0F), 0);
    }

    // ── 2. setUserDigOut(0) → C3 low 4 bits = 0 ──────────────────────────────
    // Explicit zero-value test (mirrors Task 2.1 default-state assertion at
    // a non-default-call boundary).
    // Source: Thetis networkproto1.c:601 [v2.10.3.13]
    void setUserDigOutZero_landsInLow4Bits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setUserDigOut(0);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[3]) & 0x0F), 0);
    }

    // ── 3. setUserDigOut(0x0A) → C3 low 4 bits = 0x0A ────────────────────────
    // Mid-range value with alternating bits (1010) — distinguishes the low 4
    // bits from any spurious propagation into bits 4..7.
    // Source: Thetis networkproto1.c:601 [v2.10.3.13]
    void setUserDigOutMidRange_landsInLow4Bits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setUserDigOut(0x0A);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[3]) & 0x0F), 0x0A);
    }

    // ── 4. setUserDigOut(0x0F) → C3 low 4 bits = 0x0F (max, all 4 set) ───────
    // Max value — all 4 bits set.
    // Source: Thetis networkproto1.c:601 [v2.10.3.13]
    void setUserDigOutMax_allFourBitsSet() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setUserDigOut(0x0F);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[3]) & 0x0F), 0x0F);
    }

    // ── 5. setUserDigOut(0xFF) → masks to 0x0F (low nibble only) ─────────────
    // 0xFF & 0x0F = 0x0F.  High-nibble bits MUST NOT appear on the wire — both
    // the API-boundary mask (& 0x0F in setUserDigOut) and the codec mask
    // (& 0x0F in P1CodecStandard.cpp:267) defend this invariant.
    // Source: Thetis networkproto1.c:601 [v2.10.3.13]
    void setUserDigOutAllOnes_masksToLowNibble() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setUserDigOut(0xFF);
        const QByteArray bank11 = conn.captureBank11ForTest();
        // Whole byte must equal 0x0F — bits 4..7 must be clear.
        QCOMPARE(int(quint8(bank11[3])), 0x0F);
    }

    // ── 6. setUserDigOut does NOT touch C1 (mic bits) ────────────────────────
    // Cross-bit guard.  C1 carries preamp + mic_trs (bit 4) + mic_bias (bit 5)
    // + mic_ptt (bit 6) — NONE of which user_dig_out should touch.
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13]
    void setUserDigOut_doesNotTouchC1MicBits() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        // Defaults: m_micBias=false, m_micPTT=false, m_micTipRing=true.
        // mic_trs is INVERTED on the wire so default !true = 0 in bit 4.
        // → C1 should be entirely 0.
        conn.setUserDigOut(0x0F);  // all 4 user-dig-out bits set

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C1 = bank11[1] must remain 0 (no mic-jack defaults flipped).
        QCOMPARE(int(quint8(bank11[1])), 0);
    }

    // ── 7. setUserDigOut does NOT touch C2 (line_in_gain + puresignal_run) ───
    // Cross-bit guard.  C2 carries line_in_gain (low 5 bits) + puresignal_run
    // (bit 6) — both must remain 0 when only setUserDigOut is called.
    // Source: Thetis networkproto1.c:600 [v2.10.3.13]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    void setUserDigOut_doesNotTouchC2() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setUserDigOut(0x0F);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C2 = bank11[2] must remain 0 (line_in_gain + puresignal_run defaults).
        QCOMPARE(int(quint8(bank11[2])), 0);
    }

    // ── 8. Bank 11 C0 address must remain 0x14 ───────────────────────────────
    // Source: Thetis networkproto1.c:594 [v2.10.3.13] — case 11 sets C0 |= 0x14.
    void setUserDigOut_c0AddressIs0x14() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setUserDigOut(0x05);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C0 = bank11[0]. Bits 7..1 = address 0x14; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x14.
        QCOMPARE(int(quint8(bank11[0]) & 0xFE), 0x14);
    }

    // ── 9. Round-trip: 0 → 0x07 → 0 ──────────────────────────────────────────
    void roundTrip_zeroNonZeroZero_bitsTrack() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        conn.setUserDigOut(0);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[3]) & 0x0F), 0);

        conn.setUserDigOut(0x07);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[3]) & 0x0F), 0x07);

        conn.setUserDigOut(0);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[3]) & 0x0F), 0);
    }

    // ── 10. Flush flag: setUserDigOut sets m_forceBank11Next (Codex P2) ──────
    // Reuses m_forceBank11Next from G.3 (same bank 11 byte family).  Flush
    // flag must be set BEFORE the idempotent guard (Codex P2 pattern).
    void setUserDigOut_setsForceBank11Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank11NextForTest(), false);

        conn.setUserDigOut(0x05);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 11. Flush flag set even when value doesn't change (Codex P2) ─────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // (flush flag) must still fire.
    void setUserDigOutRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setUserDigOut(0);  // no state change (default is 0)
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 12. Idempotent: setUserDigOut(0x0A) twice doesn't crash ──────────────
    void setUserDigOutTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setUserDigOut(0x0A);
        conn.setUserDigOut(0x0A);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[3]) & 0x0F), 0x0A);
    }

    // ── 13. Codec path (HL2): setUserDigOut stores but C3 unchanged ──────────
    // HL2 firmware has no Penny/Hermes Ctrl accessory header and the
    // P1CodecHl2 bank-11 case emits out[3] = 0 unconditionally (see
    // P1CodecHl2.cpp:239) — user_dig_out is a non-feature on HL2.  This
    // test pins the cross-codec contract: the setter still stores the
    // value (and the bank-11 flush flag fires) but the HL2 codec
    // deliberately does not propagate it.  Documents the intentional
    // codec divergence and prevents future regressions if someone extends
    // the HL2 codec to honour the field.
    void setUserDigOut_hl2CodecPath_c3RemainsZero() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setUserDigOut(0x0F);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // HL2 codec emits out[3] = 0 regardless of ctx.p1UserDigOut.
        QCOMPARE(int(quint8(bank11[3])), 0);
        // Flush flag still fires — Codex P2 safety contract is independent
        // of the codec's emission decision.
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }
};

QTEST_APPLESS_MAIN(TestP1UserDigOutSetter)
#include "tst_p1_user_dig_out_setter.moc"
