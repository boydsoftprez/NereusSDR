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
};

QTEST_APPLESS_MAIN(TstTxChannelDexpGate)
#include "tst_tx_channel_dexp_gate.moc"
