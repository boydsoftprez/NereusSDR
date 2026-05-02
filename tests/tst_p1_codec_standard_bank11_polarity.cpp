// no-port-check: NereusSDR-original test fixture; cites Thetis source for
// expected wire bit polarity but does not port any Thetis code.
//
// P1 full-parity Task 1.1 — codec-level direct polarity test for mic_ptt.
//
// Asserts that P1CodecStandard::composeCcForBank(11, ctx, out) emits bank 11
// C1 bit 6 (0x40) using DIRECT polarity per Thetis networkproto1.c:597-598
// [v2.10.3.13 @501e3f51]:
//   C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
//
// Pre-fix the codec wrote `!ctx.p1MicPTT` (inverted), mirroring the same bug
// PR #161 (commit ca8cd73) fixed in P1CodecHl2 for HL2.  With p1MicPTT
// default false, the inverted code emitted bit 6 = 1 every CC frame; Hermes-
// class firmware reads bit 6 as "track mic-jack tip as PTT source", so the
// floating mic tip caused phantom PTT signals fighting software MOX → rapid
// T/R relay flutter on TUNE/TX (ANAN-10E bench symptom).
//
// This test mirrors the existing P1CodecHl2 bank 11 mic_ptt test pattern and
// closes the parity gap PR #161 left open for the Standard codec (used by
// Atlas / Hermes / HermesII / Angelia / Orion / OrionMKII / AnvelinaPro3 /
// RedPitaya — every non-HL2 P1 board).

#include <QtTest/QtTest>
#include "core/codec/P1CodecStandard.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

class TestP1CodecStandardBank11Polarity : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Default p1MicPTT=false → C1 bit 6 CLEAR (direct polarity) ─────────
    // Pre-fix the codec emitted bit 6 = 1 here (inverted: !false = 1).  After
    // the P1 full-parity Task 1.1 fix, bit 6 = 0 (direct: false → 0).
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
    //   C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
    void mic_ptt_direct_polarity_default_false_emits_bit_clear() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1MicPTT = false;  // default
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x14);  // C0: bank 11 address with MOX=0
        QCOMPARE(int(out[1] & 0x40), 0x00);  // mic_ptt=false → bit 6 = 0
    }

    // ── 2. p1MicPTT=true → C1 bit 6 SET (direct polarity) ───────────────────
    // Mirror of the HL2 fix in PR #161 — the Standard codec now matches.
    // Source: Thetis networkproto1.c:597-598 [v2.10.3.13 @501e3f51]
    //   C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
    void mic_ptt_direct_polarity_true_emits_bit_set() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1MicPTT = true;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[0]), 0x14);
        QCOMPARE(int(out[1] & 0x40), 0x40);  // mic_ptt=true → bit 6 = 1
    }

    // ── 3. C2 line_in_gain — 5-bit value lands in low 5 bits ────────────────
    // Source: Thetis networkproto1.c:600 [v2.10.3.13 @501e3f51]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    // P1 full-parity Task 1.2.
    void c2_line_in_gain_in_low_5_bits() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1LineInGain = 0x17;  // 23 — fits in 5 bits
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[2] & 0x1F), 0x17);
    }

    // ── 4. C2 line_in_gain — high bits get masked off ───────────────────────
    // Source: Thetis networkproto1.c:600 [v2.10.3.13 @501e3f51]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ...
    // P1 full-parity Task 1.2.
    void c2_line_in_gain_masked_to_5_bits() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1LineInGain = 0x3F;  // 63 — high bit must be masked off
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[2] & 0x1F), 0x1F);
    }

    // ── 5. C2 puresignal_run=true → bit 6 SET ───────────────────────────────
    // Source: Thetis networkproto1.c:600 [v2.10.3.13 @501e3f51]
    //   C2 = ... | ((prn->puresignal_run & 1) << 6);
    // P1 full-parity Task 1.2.
    void c2_puresignal_run_sets_bit_6() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1PuresignalRun = true;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[2] & 0x40), 0x40);
    }

    // ── 6. C2 puresignal_run default false → bit 6 CLEAR ────────────────────
    // Source: Thetis networkproto1.c:600 [v2.10.3.13 @501e3f51]
    // P1 full-parity Task 1.2.
    void c2_puresignal_run_default_clears_bit_6() {
        P1CodecStandard codec;
        CodecContext ctx{};
        // p1PuresignalRun default = false
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[2] & 0x40), 0x00);
    }

    // ── 7. C2 combined line_in_gain + puresignal_run ────────────────────────
    // Source: Thetis networkproto1.c:600 [v2.10.3.13 @501e3f51]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    // 0x40 (puresignal bit 6) | 0x0F (line_in_gain low nibble) = 0x4F
    // P1 full-parity Task 1.2.
    void c2_combined_line_in_and_puresignal() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1LineInGain = 0x0F;
        ctx.p1PuresignalRun = true;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[2]), 0x4F);
    }

    // ── 8. C3 user_dig_out — 4-bit value lands in low 4 bits ────────────────
    // Source: Thetis networkproto1.c:601 [v2.10.3.13 @501e3f51]
    //   C3 = prn->user_dig_out & 0b00001111;
    // P1 full-parity Task 1.3.
    void c3_user_dig_out_low_4_bits() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1UserDigOut = 0x0A;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[3] & 0x0F), 0x0A);
    }

    // ── 9. C3 user_dig_out — high bits get masked off ───────────────────────
    // Source: Thetis networkproto1.c:601 [v2.10.3.13 @501e3f51]
    //   C3 = prn->user_dig_out & 0b00001111;
    // P1 full-parity Task 1.3.
    void c3_user_dig_out_masked_to_4_bits() {
        P1CodecStandard codec;
        CodecContext ctx{};
        ctx.p1UserDigOut = 0xFF;
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[3] & 0x0F), 0x0F);
    }

    // ── 10. C3 user_dig_out default zero → C3 low nibble clear ──────────────
    // Source: Thetis networkproto1.c:601 [v2.10.3.13 @501e3f51]
    // P1 full-parity Task 1.3 — confirms default p1UserDigOut=0 produces C3=0,
    // matching the prior wire bytes (regression-freeze baseline unchanged).
    void c3_user_dig_out_default_zero() {
        P1CodecStandard codec;
        CodecContext ctx{};
        // p1UserDigOut default = 0
        quint8 out[5] = {};
        codec.composeCcForBank(11, ctx, out);
        QCOMPARE(int(out[3] & 0x0F), 0x00);
    }
};

QTEST_APPLESS_MAIN(TestP1CodecStandardBank11Polarity)
#include "tst_p1_codec_standard_bank11_polarity.moc"
