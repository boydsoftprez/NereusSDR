// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_tx_channel_dexp_meters.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TxChannel DEXP meter readers (Phase 3M-3a-iii Task 6).
//
// Source references (cited for traceability; logic lives in TxChannel.cpp):
//   cmaster.cs:163-164 [v2.10.3.13]   - GetDEXPPeakSignal(int id, double*) DllImport.
//   wdsp/dexp.c:647-654 [v2.10.3.13]  - GetDEXPPeakSignal C impl: derefs pdexp[id]
//                                        under cs_update; returns a->peak (linear
//                                        amplitude, 0.0..1.0 typical).
//   console.cs:28952 [v2.10.3.13]     - picVOX_Paint caller pattern:
//                                        cmaster.GetDEXPPeakSignal(0, &audio_peak);
//                                        Then 20*Math.Log10(audio_peak) for dB.
//   console.cs:25346-25359 [v2.10.3.13] - UpdateNoiseGate async loop calling
//                                        WDSP.CalculateTXMeter(1, MIC) every 100 ms;
//                                        noise_gate_data init = -200.0f.
//   dsp.cs:992-1057 [v2.10.3.13]      - CalculateTXMeter switch maps MeterType.MIC
//                                        to GetTXAMeter(channel, TXA_MIC_AV) (= 1)
//                                        then sign-flips with -(float)val.  The
//                                        wrapper returns the RAW (unflipped) value;
//                                        callers handle the sign + 3 dB offset.
//   wdsp/TXA.h:51-52 [v2.10.3.13]     - TXA_MIC_PK=0 (peak), TXA_MIC_AV=1 (average).
//                                        Thetis CalculateTXMeter(MIC) uses TXA_MIC_AV.
//
// Test strategy: pure crash-freeness + sane bounds.  WDSP channel is never
// opened in the test binary (no OpenChannel + create_dexp), so:
//   - txa[1].rsmpin.p == nullptr  -> getTxMicMeterDb returns the floor sentinel.
//   - pdexp[1]      == nullptr  -> getDexpPeakSignal returns 0.0.
// Both wrappers MUST null-guard before dereferencing the WDSP globals or they
// would segfault under the unit-test build.
//
// Test cases (4):
//   1. getDexpPeakSignal_returnsZeroWhenIdle              - bounds [0.0, 1.0)
//   2. getDexpPeakSignal_callableMultipleTimes_noCrash    - 20 successive reads
//   3. getTxMicMeterDb_returnsFloorWhenIdle               - bounds [-200.0, 0.0]
//   4. getTxMicMeterDb_callableMultipleTimes_noCrash      - 20 successive reads
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 - New test file for Phase 3M-3a-iii Task 6: TxChannel DEXP
//                 meter readers (getDexpPeakSignal / getTxMicMeterDb).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

class TstTxChannelDexpMeters : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;  // WDSP.id(1, 0) - TX channel

private slots:

    // --------------------------------------------------------------
    // getDexpPeakSignal()  (linear amplitude 0..1)
    // --------------------------------------------------------------

    void getDexpPeakSignal_returnsZeroWhenIdle() {
        // No DSP running -> pdexp[kChannelId] == nullptr -> wrapper returns
        // the 0.0 default (matches Thetis console.cs:28951 init "double
        // audio_peak = 0.0;" before GetDEXPPeakSignal call).
        TxChannel ch(kChannelId);
        const double peak = ch.getDexpPeakSignal();
        QVERIFY(peak >= 0.0);
        QVERIFY(peak < 1.0);
    }

    void getDexpPeakSignal_callableMultipleTimes_noCrash() {
        TxChannel ch(kChannelId);
        for (int i = 0; i < 20; ++i) { (void)ch.getDexpPeakSignal(); }
    }

    // --------------------------------------------------------------
    // getTxMicMeterDb()  (raw WDSP dB; typically -200..0 dB)
    // --------------------------------------------------------------

    void getTxMicMeterDb_returnsFloorWhenIdle() {
        // No DSP running -> txa[kChannelId].rsmpin.p == nullptr -> wrapper
        // returns -200.0 (matches Thetis console.cs:25346 noise_gate_data
        // init "-200.0f" floor).  Caller is responsible for any sign-flip
        // or +3 dB offset (console.cs:25353-25354) downstream.
        TxChannel ch(kChannelId);
        const double db = ch.getTxMicMeterDb();
        QVERIFY(db <= 0.0);     // never positive
        QVERIFY(db >= -200.0);  // sane floor
    }

    void getTxMicMeterDb_callableMultipleTimes_noCrash() {
        TxChannel ch(kChannelId);
        for (int i = 0; i < 20; ++i) { (void)ch.getTxMicMeterDb(); }
    }
};

QTEST_APPLESS_MAIN(TstTxChannelDexpMeters)
#include "tst_tx_channel_dexp_meters.moc"
