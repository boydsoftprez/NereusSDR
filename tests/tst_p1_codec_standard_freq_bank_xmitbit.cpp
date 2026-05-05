// no-port-check: NereusSDR-original test fixture; cites Thetis source for
// expected wire bit behavior but does not port any Thetis code.
//
// P1 full-parity Task 1.4 — codec-level XmitBit-on-frequency-banks test.
//
// Asserts that P1CodecStandard::composeCcForBank(bank, ctx, out) emits C0
// bit 0 (the XmitBit / MOX bit) on every frequency bank when MOX is asserted,
// and clears it when MOX is deasserted.
//
// Source: Thetis ChannelMaster/networkproto1.c:447 (C0 = XmitBit before
// switch) + per-bank case statements:
//   line 477  — case 1: C0 |= 2     (TX VFO)
//   line 485  — case 2: C0 |= 4     (RX1 VFO / DDC0)
//   line 498  — case 3: C0 |= 6     (RX2 VFO / DDC1)
//   line 526  — case 5: C0 |= 8     (RX3 VFO / DDC2)
//   line 539  — case 6: C0 |= 0x0a  (RX4 VFO / DDC3)
//   line 549  — case 7: C0 |= 0x0c  (RX5 VFO / DDC4)
//   line 560  — case 8: C0 |= 0x0e  (RX6 VFO / DDC5)
//   line 569  — case 9: C0 |= 0x10  (RX7 VFO / DDC6)
// [v2.10.3.13+501e3f51]
//
// Pre-fix the standard codec hardcoded out[0] = <addr> on these eight banks
// with NO MOX bit (mirroring the pre-refactor legacy composeCcBankRxFreq /
// composeCcBankTxFreq paths).  HL2 codec (P1CodecHl2.cpp) was fixed in
// PR #161; the standard codec was missed.  This closes the parity gap for
// every non-HL2 P1 board (Atlas / Hermes / HermesII / Angelia / Orion /
// OrionMKII / AnvelinaPro3 / RedPitaya / Saturn / SaturnMKII).

#include <QtTest/QtTest>
#include "core/codec/P1CodecStandard.h"
#include "core/codec/CodecContext.h"

using namespace NereusSDR;

class TestP1CodecStandardFreqBankXmitBit : public QObject {
    Q_OBJECT

private:
    static CodecContext makeCtx(bool mox) {
        CodecContext ctx{};
        ctx.mox = mox;
        ctx.activeRxCount = 7;
        ctx.txFreqHz = 14'200'000;
        for (int i = 0; i < 7; ++i) {
            ctx.rxFreqHz[i] = 14'000'000 + i * 1000;
        }
        return ctx;
    }

    static quint8 emitBank(int bank, const CodecContext& ctx) {
        P1CodecStandard codec;
        quint8 out[5] = {};
        codec.composeCcForBank(bank, ctx, out);
        return out[0];
    }

private slots:
    // ── MOX=false → XmitBit (C0 bit 0) CLEAR on every freq bank ─────────────
    // Source: Thetis networkproto1.c:447 [v2.10.3.13]
    //   C0 = (unsigned char)XmitBit;  // XmitBit = 0 when MOX deasserted
    void mox_off_clears_xmitbit_bank1() { QCOMPARE(emitBank(1, makeCtx(false)) & 0x01, 0x00); }
    void mox_off_clears_xmitbit_bank2() { QCOMPARE(emitBank(2, makeCtx(false)) & 0x01, 0x00); }
    void mox_off_clears_xmitbit_bank3() { QCOMPARE(emitBank(3, makeCtx(false)) & 0x01, 0x00); }
    void mox_off_clears_xmitbit_bank5() { QCOMPARE(emitBank(5, makeCtx(false)) & 0x01, 0x00); }
    void mox_off_clears_xmitbit_bank6() { QCOMPARE(emitBank(6, makeCtx(false)) & 0x01, 0x00); }
    void mox_off_clears_xmitbit_bank7() { QCOMPARE(emitBank(7, makeCtx(false)) & 0x01, 0x00); }
    void mox_off_clears_xmitbit_bank8() { QCOMPARE(emitBank(8, makeCtx(false)) & 0x01, 0x00); }
    void mox_off_clears_xmitbit_bank9() { QCOMPARE(emitBank(9, makeCtx(false)) & 0x01, 0x00); }

    // ── MOX=true → XmitBit (C0 bit 0) SET on every freq bank ────────────────
    // Source: Thetis networkproto1.c:447 [v2.10.3.13]
    //   C0 = (unsigned char)XmitBit;  // XmitBit = 1 when MOX asserted
    void mox_on_sets_xmitbit_bank1() { QCOMPARE(emitBank(1, makeCtx(true)) & 0x01, 0x01); }
    void mox_on_sets_xmitbit_bank2() { QCOMPARE(emitBank(2, makeCtx(true)) & 0x01, 0x01); }
    void mox_on_sets_xmitbit_bank3() { QCOMPARE(emitBank(3, makeCtx(true)) & 0x01, 0x01); }
    void mox_on_sets_xmitbit_bank5() { QCOMPARE(emitBank(5, makeCtx(true)) & 0x01, 0x01); }
    void mox_on_sets_xmitbit_bank6() { QCOMPARE(emitBank(6, makeCtx(true)) & 0x01, 0x01); }
    void mox_on_sets_xmitbit_bank7() { QCOMPARE(emitBank(7, makeCtx(true)) & 0x01, 0x01); }
    void mox_on_sets_xmitbit_bank8() { QCOMPARE(emitBank(8, makeCtx(true)) & 0x01, 0x01); }
    void mox_on_sets_xmitbit_bank9() { QCOMPARE(emitBank(9, makeCtx(true)) & 0x01, 0x01); }

    // ── Address bits unaffected — full-byte sanity checks under MOX=true ────
    // Source: Thetis networkproto1.c:447,477,485,569 [v2.10.3.13]
    // Confirms XmitBit OR'd with the per-case address constant produces the
    // correct full C0 byte (no address-bit corruption from the fix).
    void mox_on_bank1_full_byte_is_xmitbit_or_addr() {
        QCOMPARE(int(emitBank(1, makeCtx(true))), 0x03);  // 0x02 | 0x01
    }
    void mox_on_bank2_full_byte_is_xmitbit_or_addr() {
        QCOMPARE(int(emitBank(2, makeCtx(true))), 0x05);  // 0x04 | 0x01
    }
    void mox_on_bank9_full_byte_is_xmitbit_or_addr() {
        QCOMPARE(int(emitBank(9, makeCtx(true))), 0x11);  // 0x10 | 0x01
    }
};

QTEST_GUILESS_MAIN(TestP1CodecStandardFreqBankXmitBit)
#include "tst_p1_codec_standard_freq_bank_xmitbit.moc"
