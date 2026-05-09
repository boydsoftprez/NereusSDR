// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Bank-16 puresignal_run wire-byte test for the non-HL2 P1 codec family
// (Hermes / ANAN-10 / ANAN-10E / ANAN-100 / ANAN-100B / AnvelinaPro3).
//
// **The smoking gun for the v0.4.1 bench failure on the friend's ANAN-10E.**
//
// Thetis emits the puresignal_run bit on TWO banks:
//   - Bank 11 C2 bit 6 (line 599)
//   - Bank 16 C2 bit 6 (line 663)
//
// Without the bank-16 bit, the radio firmware fires the cmaster.cs bookkeeping
// (puresignal_run flag set) but the FPGA's PA-loopback path stays disengaged
// — the feedback ADC sees only the antenna input or noise floor, never the
// PA output.  Calcc state machine reaches LCOLLECT (state=4) but the
// feedback envelope stays at the ADC noise floor (~-70 dBFS) regardless of
// TX drive level, so feedbackLevel never crosses calcc's collection
// threshold and the correction never converges.
//
// The HL2 codec was fixed for this on 2026-05-07 (PR #212 follow-up
// bench-fix, J.J. KG4VCF) — the wire-pcap of working Thetis HL2 PS-MOX
// session showed bank 16 carrying ps_run=1 in C2 bit 6.  Same fix needed
// to propagate to P1CodecStandard for Hermes-class coverage; this test
// pins that propagation.
//
// Source: Thetis ChannelMaster/networkproto1.c:657-666 [v2.10.3.13]
//   case 16: // BPF2
//       C0 |= 0x24;
//       C1 = (BPF2 HPF/LPF/preamp bits + rx2_gnd<<7);
//       C2 = (xvtr_enable & 1) | ((prn->puresignal_run & 1) << 6);
//       C3 = 0;
//       C4 = 0;
// (BPF2 / xvtr_enable not yet plumbed in NereusSDR — emit 0; only ps_run
//  is wired here.)

#include <QtTest/QtTest>
#include "core/P1RadioConnection.h"

using namespace NereusSDR;

class TestP1Bank16PuresignalRun : public QObject {
    Q_OBJECT

private:
    // Build a Hermes-class P1RadioConnection with the codec wired up.
    // setBoardForTest(HPSDRHW::Hermes) maps to HPSDRModel::HERMES which
    // selectCodec resolves to P1CodecStandard — the path the friend's
    // ANAN-10E hits in production once selectCodec runs.  Without this,
    // the default constructor leaves m_codec null and composeCcForBank
    // falls through to the frozen `composeCcForBankLegacy` rollback hatch,
    // bypassing the fix entirely.
    static QByteArray captureBank16Hermes(bool psRun) {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setPuresignalRun(psRun);
        quint8 out[5] = {};
        conn.composeCcForBankForTest(16, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }

    // Same but for HermesII (ANAN-10E / ANAN-100B path) — also routes
    // through P1CodecStandard.
    static QByteArray captureBank16HermesII(bool psRun) {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setPuresignalRun(psRun);
        quint8 out[5] = {};
        conn.composeCcForBankForTest(16, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }

private slots:
    // ── §1 Bank 16 C0 address is 0x24 (Hermes) ────────────────────────────
    // Sanity: Thetis networkproto1.c:658 — `C0 |= 0x24` for bank 16.
    void bank16_c0AddressIs0x24() {
        const QByteArray bank16 = captureBank16Hermes(/*psRun=*/false);
        QCOMPARE(bank16.size(), 5);
        // C0 = bank16[0]. MOX defaults false → C0 should be exactly 0x24
        // (no MOX bit in C0 bit 0).
        QCOMPARE(int(quint8(bank16[0]) & 0xFE), 0x24);
    }

    // ── §2 puresignal_run=false → C2 bit 6 CLEAR (Hermes) ─────────────────
    void puresignalRunFalse_c2Bit6IsClear_hermes() {
        const QByteArray bank16 = captureBank16Hermes(/*psRun=*/false);
        QCOMPARE(int(quint8(bank16[2]) & 0x40), 0);
    }

    // ── §3 puresignal_run=true → C2 bit 6 SET (Hermes) ────────────────────
    // **The regression test for the bench failure.**
    // Without this bit on bank 16, the FPGA PA-loopback path on Hermes-class
    // P1 boards stays disengaged even though bank 11 carries the same bit.
    // Effect: calcc reaches LCOLLECT but feedback samples never have signal.
    void puresignalRunTrue_c2Bit6IsSet_hermes() {
        const QByteArray bank16 = captureBank16Hermes(/*psRun=*/true);
        QCOMPARE(int(quint8(bank16[2]) & 0x40), 0x40);
    }

    // ── §4 puresignal_run=true → C2 bit 6 SET (HermesII / ANAN-10E) ───────
    // The friend's ANAN-10E specifically — same dispatch via P1CodecStandard.
    void puresignalRunTrue_c2Bit6IsSet_hermesII() {
        const QByteArray bank16 = captureBank16HermesII(/*psRun=*/true);
        QCOMPARE(int(quint8(bank16[2]) & 0x40), 0x40);
    }

    // ── §5 puresignal_run does NOT clobber other C2 bits ──────────────────
    // C2 bit 0 = xvtr_enable (not yet plumbed, must remain 0).
    // Bits 1-5 reserved.  Bit 7 reserved.  Only bit 6 should toggle on the
    // ps_run flip.
    void puresignalRunFlip_doesNotClobberOtherC2Bits() {
        const QByteArray bank16 = captureBank16Hermes(/*psRun=*/true);
        // Mask out bit 6, the rest must be 0 (xvtr / reserved bits).
        QCOMPARE(int(quint8(bank16[2]) & ~0x40), 0);
    }

    // ── §6 Bank 16 C3 + C4 stay at 0 ─────────────────────────────────────
    // Thetis sets C3 = C4 = 0; we must too.
    void bank16_c3AndC4StayZero() {
        const QByteArray bank16 = captureBank16Hermes(/*psRun=*/true);
        QCOMPARE(int(quint8(bank16[3])), 0);
        QCOMPARE(int(quint8(bank16[4])), 0);
    }
};

QTEST_MAIN(TestP1Bank16PuresignalRun)
#include "tst_p1_bank16_pursignal_run.moc"
