// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_tx_channel_dexp_gate.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TxChannel DEXP gate / ratio / side-channel-filter /
// audio-look-ahead setters (Phase 3M-3a-iii Tasks 3-5).
//
// Source references (cited for traceability; logic lives in TxChannel.cpp):
//   cmaster.cs:181-185 [v2.10.3.13] - SetDEXPExpansionRatio,
//     SetDEXPHysteresisRatio DllImports.
//   cmaster.cs:190-197 [v2.10.3.13] - SetDEXPLowCut, SetDEXPHighCut,
//     SetDEXPRunSideChannelFilter DllImports.
//   cmaster.cs:202-206 [v2.10.3.13] - SetDEXPRunAudioDelay,
//     SetDEXPAudioDelay DllImports.
//   setup.cs:18915-18924 [v2.10.3.13] - Math.Pow(10.0, dB/20.0)
//     conversion for ExpansionRatio (positive sign);
//     Math.Pow(10.0, -dB/20.0) for HysteresisRatio (NEGATIVE sign).
//   setup.cs:18933-18948 [v2.10.3.13] - chkSCFEnable_CheckedChanged,
//     udSCFLowCut_ValueChanged, udSCFHighCut_ValueChanged.
//   setup.cs:18951-18962 [v2.10.3.13] - chkDEXPLookAheadEnable_CheckedChanged,
//     udDEXPLookAhead_ValueChanged (ms->seconds /1000.0).
//   setup.Designer.cs:44765-44818 [v2.10.3.13] - udDEXPLookAhead 10..999 ms
//     default 60; chkDEXPLookAheadEnable default CHECKED (true).
//   setup.Designer.cs:44845-44874 [v2.10.3.13] - udDEXPHysteresisRatio
//     0.0..10.0 dB default 2.0 step 0.1 (DecimalPlaces=1, raw value 20).
//   setup.Designer.cs:44876-44905 [v2.10.3.13] - udDEXPExpansionRatio
//     0.0..30.0 dB default 10.0 step 0.1 (DecimalPlaces=1, raw value 10
//     with no scale shift = 10.0 dB).
//   setup.Designer.cs:45187-45260 [v2.10.3.13] - udSCFHighCut /
//     udSCFLowCut 100..10000 Hz (defaults 1500 / 500); chkSCFEnable
//     default CHECKED.
//
// Tests verify (NEREUS_BUILD_TESTS test-seam accessors required):
//   - First call stores the value (NaN sentinel fires on doubles).
//   - Round-trip / clamp at the wrapper boundary (Thetis ranges).
//   - Idempotent guard: second identical call is observable as the
//     stored value.
//   - For ratio setters: the wrapper STORES the dB value (idempotency
//     check) but PUSHES the linear Math.Pow value to WDSP - tests
//     inspect the stored dB.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 - New test file for Phase 3M-3a-iii Tasks 3-5: DEXP
//                 ratio (Expansion + Hysteresis) + side-channel filter
//                 (LowCut + HighCut + RunSideChannelFilter) + audio
//                 look-ahead (RunAudioDelay + AudioDelay) wrappers.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#define NEREUS_BUILD_TESTS 1

#include <QtTest/QtTest>
#include <cmath>   // std::isnan

#include "core/TxChannel.h"

using namespace NereusSDR;

class TstTxChannelDexpGate : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;  // WDSP.id(1, 0) - TX channel

private slots:

    // --------------------------------------------------------------
    // setDexpExpansionRatio  (range 0..30 dB, default 10.0)
    // setup.Designer.cs:44876-44905 [v2.10.3.13]
    //
    // Wrapper takes dB; Thetis converts to linear via
    // Math.Pow(10.0, dB/20.0)  -- POSITIVE sign
    // (setup.cs:18915-18919 [v2.10.3.13]).
    // The wrapper STORES the dB value (idempotency check), but PUSHES
    // the linear value to WDSP.  Test inspects the stored dB.
    // --------------------------------------------------------------

    void setDexpExpansionRatio_inRange_dbToLinear() {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpExpansionRatioDbForTest()));
        ch.setDexpExpansionRatio(1.0);                // 1 dB
        QCOMPARE(ch.lastDexpExpansionRatioDbForTest(), 1.0);
        ch.setDexpExpansionRatio(15.0);
        QCOMPARE(ch.lastDexpExpansionRatioDbForTest(), 15.0);
    }
    void setDexpExpansionRatio_clampedLow() {
        TxChannel ch(kChannelId);
        ch.setDexpExpansionRatio(-1.0);   // Below 0.0
        QCOMPARE(ch.lastDexpExpansionRatioDbForTest(), 0.0);
    }
    void setDexpExpansionRatio_clampedHigh() {
        TxChannel ch(kChannelId);
        ch.setDexpExpansionRatio(40.0);   // Above 30.0
        QCOMPARE(ch.lastDexpExpansionRatioDbForTest(), 30.0);
    }
    void setDexpExpansionRatio_idempotent() {
        TxChannel ch(kChannelId);
        ch.setDexpExpansionRatio(2.0);
        ch.setDexpExpansionRatio(2.0);
        QCOMPARE(ch.lastDexpExpansionRatioDbForTest(), 2.0);
    }

    // --------------------------------------------------------------
    // setDexpHysteresisRatio  (range 0..10 dB, default 2.0)
    // setup.Designer.cs:44845-44874 [v2.10.3.13]
    //
    // Thetis uses NEGATIVE sign in Math.Pow:
    //   Math.Pow(10.0, -dB/20.0)
    // (setup.cs:18921-18925 [v2.10.3.13]).  The wrapper still stores
    // the user-facing dB; the Math.Pow happens at the WDSP boundary.
    // --------------------------------------------------------------

    void setDexpHysteresisRatio_inRange() {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpHysteresisRatioDbForTest()));
        ch.setDexpHysteresisRatio(2.0);
        QCOMPARE(ch.lastDexpHysteresisRatioDbForTest(), 2.0);
        ch.setDexpHysteresisRatio(5.5);
        QCOMPARE(ch.lastDexpHysteresisRatioDbForTest(), 5.5);
    }
    void setDexpHysteresisRatio_clampedLow() {
        TxChannel ch(kChannelId);
        ch.setDexpHysteresisRatio(-1.0);
        QCOMPARE(ch.lastDexpHysteresisRatioDbForTest(), 0.0);
    }
    void setDexpHysteresisRatio_clampedHigh() {
        TxChannel ch(kChannelId);
        ch.setDexpHysteresisRatio(15.0);
        QCOMPARE(ch.lastDexpHysteresisRatioDbForTest(), 10.0);
    }

    // --------------------------------------------------------------
    // setDexpLowCut / setDexpHighCut / setDexpRunSideChannelFilter
    // (Phase 3M-3a-iii Task 4)
    //
    // Thetis Setup UI: VOX/DEXP page → "Side Channel Filter" group
    //   chkSCFEnable             default CHECKED  (setup.Designer.cs:45250)
    //   udSCFLowCut    100..10000 Hz default 500   (setup.Designer.cs:45219-45245)
    //   udSCFHighCut   100..10000 Hz default 1500  (setup.Designer.cs:45189-45215)
    // Call-sites: setup.cs:18933-18948 [v2.10.3.13]
    //   chkSCFEnable_CheckedChanged  → SetDEXPRunSideChannelFilter
    //   udSCFLowCut_ValueChanged     → SetDEXPLowCut    (Hz, no unit conv)
    //   udSCFHighCut_ValueChanged    → SetDEXPHighCut   (Hz, no unit conv)
    // --------------------------------------------------------------

    void setDexpLowCut_inRange() {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpLowCutHzForTest()));
        ch.setDexpLowCut(500.0);
        QCOMPARE(ch.lastDexpLowCutHzForTest(), 500.0);
        ch.setDexpLowCut(2000.0);
        QCOMPARE(ch.lastDexpLowCutHzForTest(), 2000.0);
    }
    void setDexpLowCut_clampedLow() {
        // Below Thetis minimum of 100 Hz gets clamped to 100.
        TxChannel ch(kChannelId);
        ch.setDexpLowCut(-50.0);
        QCOMPARE(ch.lastDexpLowCutHzForTest(), 100.0);
        ch.setDexpLowCut(50.0);
        QCOMPARE(ch.lastDexpLowCutHzForTest(), 100.0);
    }
    void setDexpLowCut_clampedHigh() {
        // Above Thetis maximum of 10000 Hz gets clamped to 10000.
        TxChannel ch(kChannelId);
        ch.setDexpLowCut(25000.0);
        QCOMPARE(ch.lastDexpLowCutHzForTest(), 10000.0);
    }
    void setDexpLowCut_idempotent() {
        TxChannel ch(kChannelId);
        ch.setDexpLowCut(500.0);
        ch.setDexpLowCut(500.0);
        QCOMPARE(ch.lastDexpLowCutHzForTest(), 500.0);
    }
    void setDexpHighCut_inRange() {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpHighCutHzForTest()));
        ch.setDexpHighCut(1500.0);
        QCOMPARE(ch.lastDexpHighCutHzForTest(), 1500.0);
        ch.setDexpHighCut(5000.0);
        QCOMPARE(ch.lastDexpHighCutHzForTest(), 5000.0);
    }
    void setDexpHighCut_clampedLow() {
        TxChannel ch(kChannelId);
        ch.setDexpHighCut(-10.0);
        QCOMPARE(ch.lastDexpHighCutHzForTest(), 100.0);
    }
    void setDexpHighCut_clampedHigh() {
        TxChannel ch(kChannelId);
        ch.setDexpHighCut(15000.0);
        QCOMPARE(ch.lastDexpHighCutHzForTest(), 10000.0);
    }
    void setDexpRunSideChannelFilter_idempotent() {
        TxChannel ch(kChannelId);
        // Default is false (cache init); Thetis ships chkSCFEnable CHECKED
        // but the wrapper-side cache initialises to false to match the
        // create_dexp WDSP boot state (a->run_filt = 0).  Caller (UI/model)
        // is responsible for pushing true at startup if Thetis-default
        // behavior is desired.
        QCOMPARE(ch.lastDexpRunSideChannelFilterForTest(), false);
        ch.setDexpRunSideChannelFilter(true);
        QCOMPARE(ch.lastDexpRunSideChannelFilterForTest(), true);
        ch.setDexpRunSideChannelFilter(true);   // idempotent re-call
        QCOMPARE(ch.lastDexpRunSideChannelFilterForTest(), true);
        ch.setDexpRunSideChannelFilter(false);
        QCOMPARE(ch.lastDexpRunSideChannelFilterForTest(), false);
    }

    // --------------------------------------------------------------
    // setDexpRunAudioDelay (audio look-ahead master enable)
    // setDexpAudioDelay    (range 10..999 ms, default 60)
    // (Phase 3M-3a-iii Task 5)
    //
    // Thetis Setup UI: VOX/DEXP page → "Audio LookAhead" group
    //   chkDEXPLookAheadEnable  default CHECKED   (setup.Designer.cs:44808)
    //   udDEXPLookAhead         10..999 ms def 60 (setup.Designer.cs:44765-44793)
    //
    // Thetis ships chkDEXPLookAheadEnable as DEFAULT TRUE: the look-ahead
    // buffer lets VOX fire just BEFORE the first syllable instead of
    // clipping it.  But Thetis also AND's the user check with vox/dexp
    // enable (setup.cs:18954: enable = chkDEXPLookAheadEnable.Checked &&
    // (chkVOXEnable.Checked || chkDEXPEnable.Checked)) — caller is
    // responsible for that gating; the wrapper just pushes the bool.
    //
    // Cache init false matches WDSP create_dexp (a->run_audelay = 0).
    // --------------------------------------------------------------

    void setDexpRunAudioDelay_idempotent() {
        TxChannel ch(kChannelId);
        QCOMPARE(ch.lastDexpRunAudioDelayForTest(), false);
        ch.setDexpRunAudioDelay(true);
        QCOMPARE(ch.lastDexpRunAudioDelayForTest(), true);
        ch.setDexpRunAudioDelay(true);   // idempotent re-call
        QCOMPARE(ch.lastDexpRunAudioDelayForTest(), true);
        ch.setDexpRunAudioDelay(false);
        QCOMPARE(ch.lastDexpRunAudioDelayForTest(), false);
    }
    void setDexpAudioDelay_inRange() {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastDexpAudioDelayMsForTest()));
        ch.setDexpAudioDelay(60.0);
        QCOMPARE(ch.lastDexpAudioDelayMsForTest(), 60.0);
        ch.setDexpAudioDelay(500.0);
        QCOMPARE(ch.lastDexpAudioDelayMsForTest(), 500.0);
    }
    void setDexpAudioDelay_clampedLow() {
        // Below Thetis minimum of 10 ms gets clamped to 10.
        TxChannel ch(kChannelId);
        ch.setDexpAudioDelay(5.0);
        QCOMPARE(ch.lastDexpAudioDelayMsForTest(), 10.0);
    }
    void setDexpAudioDelay_clampedHigh() {
        // Above Thetis maximum of 999 ms gets clamped to 999.
        TxChannel ch(kChannelId);
        ch.setDexpAudioDelay(2000.0);
        QCOMPARE(ch.lastDexpAudioDelayMsForTest(), 999.0);
    }
    void setDexpAudioDelay_idempotent() {
        TxChannel ch(kChannelId);
        ch.setDexpAudioDelay(60.0);
        ch.setDexpAudioDelay(60.0);
        QCOMPARE(ch.lastDexpAudioDelayMsForTest(), 60.0);
    }
};

QTEST_APPLESS_MAIN(TstTxChannelDexpGate)
#include "tst_tx_channel_dexp_gate.moc"
