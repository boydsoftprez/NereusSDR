// no-port-check: test-only — Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// Phase 3M-4 mi0bot audit follow-up: per-board PS DDC pair verification.
//
// Each per-board codec's applyPureSignalDdcConfig() must populate
// PsDdcConfig::psFbDdc / .txMonDdc with the DDC indices that match the
// Thetis MetisReadThreadMainLoop dispatch (networkproto1.c switch(nddc))
// and the GetDDC() table (console.cs:8579+):
//
//   nddc=2 (HermesII):              psFbDdc=0, txMonDdc=1
//   nddc=4 (HL2/Hermes/ANAN10/100): psFbDdc=2, txMonDdc=3
//   nddc=5 P1 (Orion-class P1):     psFbDdc=3, txMonDdc=4
//   nddc=5 P2 (Saturn-class P2):    psFbDdc=0, txMonDdc=1 (network.c override)
//   nddc=5 RedPitaya P1:            psFbDdc=3, txMonDdc=4 (same as G2-class P1)
//
// Test strategy: instantiate each per-board codec, call
// applyPureSignalDdcConfig() with the (mox=true, ps=true) condition that
// triggers the PS-on branch, assert psFbDdc/txMonDdc match Thetis source.

#include <QtTest/QtTest>

#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P1CodecStandard.h"
#include "core/codec/P2CodecOrionMkII.h"

using namespace NereusSDR;

class TestPsDdcIndicesPerBoard : public QObject {
    Q_OBJECT

private slots:

    // ── 1. P1 HermesII (nddc=2) → psFbDdc=0, txMonDdc=1 ─────────────────────
    //
    // Source: mi0bot networkproto1.c:380-381 [v2.10.3.13-beta2]
    //   case 2: twist(spr, 0, 1, 0); break;     // pair DDC0+DDC1
    // + console.cs:8798-8800 [v2.10.3.13-beta2] GetDDC HermesII case 5:
    //   psrx = 0; pstx = 1;
    void p1_hermesII_psMox_indicesAreZeroAndOne() {
        P1CodecStandard codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN10E,
            /*psEnabled=*/true,
            /*diversityEnabled=*/false,
            /*moxState=*/true,
            /*rx1Rate=*/192000,
            /*rx2Rate=*/192000,
            /*rx2Enabled=*/false,
            /*adcCtrl1=*/0,
            /*adcCtrl2=*/0);

        QCOMPARE(cfg.nDdc, 2);
        QCOMPARE(cfg.psFbDdc, 0);
        QCOMPARE(cfg.txMonDdc, 1);
    }

    // ── 2. P1 plain Hermes (nddc=4) → psFbDdc=2, txMonDdc=3 ─────────────────
    //
    // Source: mi0bot networkproto1.c:383-387 [v2.10.3.13-beta2]
    //   case 4: xrouter(0, 0, 0, spr, prn->RxBuff[0]);
    //           twist(spr, 2, 3, 1);            // pair DDC2+DDC3
    //           xrouter(0, 0, 2, spr, prn->RxBuff[1]);
    // + console.cs Hermes/HL2 P1 PS-MOX case 5: rx1=0, rx2=1, psrx=2, pstx=3.
    void p1_hermes_psMox_indicesAreTwoAndThree() {
        P1CodecStandard codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMES,
            /*psEnabled=*/true,
            /*diversityEnabled=*/false,
            /*moxState=*/true,
            /*rx1Rate=*/192000,
            /*rx2Rate=*/192000,
            /*rx2Enabled=*/false,
            /*adcCtrl1=*/0,
            /*adcCtrl2=*/0);

        QCOMPARE(cfg.nDdc, 4);
        QCOMPARE(cfg.psFbDdc, 2);
        QCOMPARE(cfg.txMonDdc, 3);
    }

    // ── 3. P1 HL2 (nddc=4) → psFbDdc=2, txMonDdc=3 ──────────────────────────
    //
    // Source: mi0bot networkproto1.c:549-553 [v2.10.3.13-beta2]
    // MetisReadThreadMainLoop_HL2 case 4 (byte-identical to standard case 4):
    //   xrouter(0, 0, 0, spr, prn->RxBuff[0]);
    //   twist(spr, 2, 3, 1);
    //   xrouter(0, 0, 2, spr, prn->RxBuff[1]);
    // + console.cs:8757-8762 [v2.10.3.13-beta2] HL2 P1 case 5:
    //   rx1=0, rx2=1, psrx=2, pstx=3.
    void p1_hl2_psMox_indicesAreTwoAndThree() {
        P1CodecHl2 codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            /*psEnabled=*/true,
            /*diversityEnabled=*/false,
            /*moxState=*/true,
            /*rx1Rate=*/192000,
            /*rx2Rate=*/192000,
            /*rx2Enabled=*/false,
            /*adcCtrl1=*/0,
            /*adcCtrl2=*/0);

        QCOMPARE(cfg.nDdc, 4);
        QCOMPARE(cfg.psFbDdc, 2);
        QCOMPARE(cfg.txMonDdc, 3);
    }

    // ── 4. P1 G2-class (nddc=5) → psFbDdc=3, txMonDdc=4 ─────────────────────
    //
    // Source: mi0bot networkproto1.c:388-392 [v2.10.3.13-beta2]
    //   case 5: twist(spr, 0, 1, 0);            // sync receivers
    //           twist(spr, 3, 4, 1);            // pair DDC3+DDC4 → PS
    //           xrouter(0, 0, 2, spr, prn->RxBuff[2]);
    // + console.cs:8710-8714 [v2.10.3.13-beta2] Orion-class P1 case 5:
    //   rx1=0, rx2=2, psrx=3, pstx=4.
    void p1_g2Class_psMox_indicesAreThreeAndFour() {
        P1CodecStandard codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN_G2,
            /*psEnabled=*/true,
            /*diversityEnabled=*/false,
            /*moxState=*/true,
            /*rx1Rate=*/192000,
            /*rx2Rate=*/192000,
            /*rx2Enabled=*/false,
            /*adcCtrl1=*/0,
            /*adcCtrl2=*/0);

        QCOMPARE(cfg.nDdc, 5);
        QCOMPARE(cfg.psFbDdc, 3);
        QCOMPARE(cfg.txMonDdc, 4);
    }

    // ── 5. P1 RedPitaya (nddc=5) → psFbDdc=3, txMonDdc=4 ────────────────────
    //
    // Source: same MetisReadThreadMainLoop case 5 dispatch as other nddc=5
    // P1 boards.  RedPitaya P1 takes its own console.cs branch but the
    // read-loop is shared, so the PS DDC pair is the same.
    void p1_redPitaya_psMox_indicesAreThreeAndFour() {
        P1CodecRedPitaya codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::REDPITAYA,
            /*psEnabled=*/true,
            /*diversityEnabled=*/false,
            /*moxState=*/true,
            /*rx1Rate=*/192000,
            /*rx2Rate=*/192000,
            /*rx2Enabled=*/false,
            /*adcCtrl1=*/0,
            /*adcCtrl2=*/0);

        QCOMPARE(cfg.nDdc, 5);
        QCOMPARE(cfg.psFbDdc, 3);
        QCOMPARE(cfg.txMonDdc, 4);
    }

    // ── 6. P2 Saturn-class (nddc=5) → psFbDdc=0, txMonDdc=1 ─────────────────
    //
    // Source: P2 differs from P1.  network.c:936-945 [v2.10.3.13] forces
    // DDC0+DDC1 to TX freq during PS-MOX, making them the implicit PS pair
    // regardless of the DDC count encoded by the C# layer.  console.cs P2
    // case 5: rx1=2, rx2=3 (no explicit psrx/pstx — implicit DDC0/DDC1).
    //
    // Matches PsccPump default cmaster.cs:533-534 [v2.10.3.13].
    void p2_saturn_psMox_indicesAreZeroAndOne() {
        P2CodecOrionMkII codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::ANAN_G2,
            /*psEnabled=*/true,
            /*diversityEnabled=*/false,
            /*moxState=*/true,
            /*rx1Rate=*/192000,
            /*rx2Rate=*/192000,
            /*rx2Enabled=*/false,
            /*adcCtrl1=*/0,
            /*adcCtrl2=*/0);

        QCOMPARE(cfg.nDdc, 5);
        QCOMPARE(cfg.psFbDdc, 0);
        QCOMPARE(cfg.txMonDdc, 1);
    }

    // ── 7. Default PsDdcConfig (no PS) → psFbDdc=-1, txMonDdc=-1 ────────────
    //
    // Sanity: PsDdcConfig default-initializes to the no-pair sentinel. Codec
    // branches that don't trigger the PS-on path leave it in place, which is
    // what tells a consumer "PureSignal is not running" as distinct from
    // "PureSignal is parked on Stream0/Stream1". Those are different states
    // and (0, 1) cannot express both: the every-frame paired emit during
    // ordinary RX (fixed 2026-08-01) is what happens when they are conflated.
    void defaultPsDdcConfig_indicesAreTheNoPairSentinel() {
        PsDdcConfig cfg;
        QCOMPARE(cfg.psFbDdc, -1);
        QCOMPARE(cfg.txMonDdc, -1);
    }

    // ── 8. P1 HL2 RX-only (mox=false): sentinel preserved ──────────────────
    //
    // Codec only sets psFbDdc/txMonDdc in the PS-on branch (mox && ps).
    // Outside that branch the no-pair sentinel is returned.
    void p1_hl2_rxOnly_indicesAreTheNoPairSentinel() {
        P1CodecHl2 codec;
        const PsDdcConfig cfg = codec.applyPureSignalDdcConfig(
            HPSDRModel::HERMESLITE,
            /*psEnabled=*/false,
            /*diversityEnabled=*/false,
            /*moxState=*/false,
            /*rx1Rate=*/192000,
            /*rx2Rate=*/192000,
            /*rx2Enabled=*/false,
            /*adcCtrl1=*/0,
            /*adcCtrl2=*/0);

        QCOMPARE(cfg.psFbDdc, -1);
        QCOMPARE(cfg.txMonDdc, -1);
    }
};

QTEST_APPLESS_MAIN(TestPsDdcIndicesPerBoard)
#include "tst_ps_ddc_indices_per_board.moc"
