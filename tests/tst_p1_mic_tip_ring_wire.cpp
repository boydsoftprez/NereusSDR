// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Wire-byte snapshot tests for P1RadioConnection::setMicTipRing() (3M-1b Task G.3).
//
// mic_trs bit position: bank 11 (C0=0x14) C1 byte, bit 4 (mask 0x10).
// POLARITY INVERSION: parameter tipHot = true means "Tip is mic" (intuitive).
// Wire bit is INVERTED: 0 = Tip-is-mic (tipHot=true), 1 = Tip-is-BIAS (tipHot=false).
//
// Source cite:
//   Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
//     case 11: C1 = (prn->rx[0].preamp & 1) | ((prn->rx[1].preamp & 1) << 1) |
//                   ((prn->rx[2].preamp & 1) << 2) | ((prn->rx[0].preamp & 1) << 3) |
//                   ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5) |
//                   ((prn->mic.mic_ptt & 1) << 6);
//     → mic_trs is bit 4 (mask 0x10) of C1 in bank 11.
//     → mic_trs = 1 means Tip is BIAS/PTT (ring is mic) — inverted from tipHot.
//
// Bank 11 byte layout for C0=0x14:
//   out[0] = 0x14 (C0 — address ORed with MOX bit 0)
//   out[1] = C1: preamp bits 0-3 | mic_trs bit 4 (INVERTED) | mic_bias bit 5 | mic_ptt bit 6
//   out[2] = line_in_gain + puresignal
//   out[3] = user digital outputs
//   out[4] = ADC0 RX step ATT (5-bit + 0x20 enable)
//
// Test seam: captureBank11ForTest() calls composeCcForBankForTest(11, ...)
// which routes through composeCcForBank(). Both the codec path (when
// setBoardForTest() is called) and the legacy path are exercised.
#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1MicTipRingWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default state: mic_trs bit is CLEAR (tip-is-mic, default tipHot=true) ──
    // m_micTipRing defaults true (Tip is mic), so wire bit must be 0
    // (mic_trs = 0 → ring is BIAS/PTT, tip is mic).
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    //   ((prn->mic.mic_trs & 1) << 4) → bit is 0 when mic_trs = 0.
    void defaultState_micTrsBitIsClear() {
        P1RadioConnection conn;
        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C1 = bank11[1], bit 4 (0x10) must be 0 when tipHot=true (default).
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0);
    }

    // ── 2. setMicTipRing(true) [tip-is-mic] → C1 bit 4 CLEAR ────────────────
    // tipHot = true → Tip is mic → mic_trs = 0 → wire bit 4 = 0.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    void setMicTipRingTrue_c1Bit4IsClear() {
        P1RadioConnection conn;
        conn.setMicTipRing(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 4 (0x10) must be 0 (Tip is mic → mic_trs = 0).
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0);
    }

    // ── 3. setMicTipRing(false) [tip-is-bias] → C1 bit 4 SET ────────────────
    // tipHot = false → Tip is BIAS/PTT → mic_trs = 1 → wire bit 4 = 1.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    //   ((prn->mic.mic_trs & 1) << 4) → 0x10 when mic_trs = 1.
    void setMicTipRingFalse_c1Bit4IsSet() {
        P1RadioConnection conn;
        conn.setMicTipRing(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 4 (0x10) must be 1 (Tip is BIAS → mic_trs = 1).
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0x10);
    }

    // ── 4. Round-trip: setMicTipRing(false) then setMicTipRing(true) ─────────
    // Verify the bit clears after being set.
    void roundTrip_falseToTrue_bitClears() {
        P1RadioConnection conn;

        conn.setMicTipRing(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x10), 0x10);

        conn.setMicTipRing(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[1]) & 0x10), 0);
    }

    // ── 5. Bank 11 C0 address must be 0x14 ───────────────────────────────────
    // Bank 11's C0 byte carries address 0x14 (bits 7..1).  After setMicTipRing,
    // C1 changes but C0 must still have address 0x14.
    // Source: Thetis networkproto1.c:594 [v2.10.3.13] — C0 |= 0x14
    void c0AddressIs0x14() {
        P1RadioConnection conn;
        conn.setMicTipRing(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C0 = bank11[0].  Bits 7..1 = address 0x14; bit 0 = MOX.
        // MOX defaults false → C0 should be exactly 0x14.
        QCOMPARE(int(quint8(bank11[0]) & 0xFE), 0x14);
    }

    // ── 6. setMicTipRing does NOT clobber C1 bits 0-3 (preamp bits) ──────────
    // Preamp bits 0-3 must be unaffected when setMicTipRing(false) sets bit 4.
    // Source: Thetis networkproto1.c:595-598 [v2.10.3.13]
    //   C1 = (prn->rx[0].preamp & 1) | ... | ((prn->mic.mic_trs & 1) << 4) | ...
    void setMicTipRingFalse_doesNotClobberPreampBits() {
        P1RadioConnection conn;
        conn.setMicTipRing(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Post issue #182: bit 4 (0x10) is set by setMicTipRing(false); bit 6
        // (mic_ptt direct, default disabled=false) is CLEAR. Bits 0-3 and bit 5
        // are 0. Total C1 should equal 0x10.
        // Source: Thetis console.cs:19757 [v2.10.3.13+501e3f51]:
        //   private bool mic_ptt_disabled = false;
        QCOMPARE(int(quint8(bank11[1])), 0x10);
    }

    // ── 7. setMicTipRing(true) leaves C1 entirely zero in default state ─────
    // Post issue #182: with tipHot=true, mic_ptt direct (default false), bias
    // default false, no preamps set, C1 = 0x00.
    void setMicTipRingTrue_c1BitsAllZero() {
        P1RadioConnection conn;
        conn.setMicTipRing(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // mic_trs=0 (tipHot=true → !true=0), mic_bias=0, mic_ptt direct=0.
        QCOMPARE(int(quint8(bank11[1])), 0x00);
    }

    // ── 8. Flush flag: setMicTipRing sets m_forceBank11Next ──────────────────
    // setMicTipRing() must set m_forceBank11Next (mirrors setMicBoost/setLineIn
    // Codex P2 pattern: safety effect fires before idempotent guard).
    void setMicTipRing_setsForceBank11Next() {
        P1RadioConnection conn;

        // Initially the flush flag must be false.
        QCOMPARE(conn.forceBank11NextForTest(), false);

        conn.setMicTipRing(false);
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 9. Flush flag set even when value doesn't change (Codex P2) ──────────
    // Codex P2: even when the stored value doesn't change, the safety effect
    // (flush flag) must still fire.
    void setMicTipRingRepeat_stillSetsForceFlag() {
        P1RadioConnection conn;
        conn.setMicTipRing(true);  // no state change (default is true)
        QCOMPARE(conn.forceBank11NextForTest(), true);
    }

    // ── 10. Idempotent: setMicTipRing(false) twice doesn't crash ─────────────
    void setMicTipRingFalseTwice_doesNotCrash() {
        P1RadioConnection conn;
        conn.setMicTipRing(false);
        conn.setMicTipRing(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0x10);
    }

    // ── 11. Codec path (Standard): setMicTipRing(false) → C1 bit 4 set ──────
    // setBoardForTest(HermesII) installs P1CodecStandard (m_codec != null).
    // The codec path must read ctx.p1MicTipRing and set bit 4 of C1.
    // Source: Thetis networkproto1.c:597 [v2.10.3.13]
    void setMicTipRingFalse_codecPath_c1Bit4Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicTipRing(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0x10);
    }

    // ── 12. Codec path (Standard): setMicTipRing(true) → C1 bit 4 clear ─────
    void setMicTipRingTrue_codecPath_c1Bit4Clear() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // → P1CodecStandard
        conn.setMicTipRing(false);
        conn.setMicTipRing(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0);
    }

    // ── 13. Codec path (HL2): setMicTipRing(false) → C1 bit 4 set ───────────
    // HL2 firmware does not have a mic jack — but P1CodecHl2::bank11 still
    // writes ctx.p1MicTipRing for correctness; the HL2 FW ignores the bit.
    void setMicTipRingFalse_hl2CodecPath_c1Bit4Set() {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesLite);  // → P1CodecHl2
        conn.setMicTipRing(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0x10);
    }

    // ── 14. setMicTipRing(false) does NOT touch C1 bits 0-3 (preamp bits) ────
    // Cross-bit guard: mic_trs (bit 4 = 0x10) must not collide with the
    // per-ADC preamp bits (bits 0-3). Both field groups are independent.
    // Source: Thetis networkproto1.c:595-598 [v2.10.3.13]
    void setMicTipRingFalse_doesNotTouchPreampBits() {
        P1RadioConnection conn;
        // Only set mic tip ring — preamp bits must remain 0.
        conn.setMicTipRing(false);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // Bit 4 (mic_trs) must be 1.
        QCOMPARE(int(quint8(bank11[1]) & 0x10), 0x10);
        // Bits 0-3 (preamp) must all be 0 — not set by setMicTipRing.
        QCOMPARE(int(quint8(bank11[1]) & 0x0F), 0);
    }
};

QTEST_APPLESS_MAIN(TestP1MicTipRingWire)
#include "tst_p1_mic_tip_ring_wire.moc"
