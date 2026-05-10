// no-port-check: NereusSDR-original test file. Cites Thetis line numbers
// as verification anchors (we assert that NereusSDR defaults + behavior
// match Thetis verbatim), not as port-from references. No verbatim Thetis
// source is reproduced here beyond brief default-value quotes used as test
// oracles.
//
// tst_tx_analyzer_settings.cpp
//
// Phase 3M-5d: TX FFT + Detector + Averaging. Pins the 9-control state
// surface added to TxAnalyzer plus the n_pixout=2 architectural change.
// Tests reference master plan §Phase 3 at
// docs/architecture/tx-display-settings-master-plan.md.
//
// SHOW (Thetis sources read for this test):
//
// 1. specHPSDR.cs:134 [v2.10.3.13+501e3f51], window_type default:
//      private int window_type = 4;
//    Index 4 maps to "Hamming" per comboTXDispWinType item ordering at
//    setup.designer.cs:36555-36562. 3M-5d reverts the 3M-5b
//    NereusSDR-architectural divergence (BH4 = 1) back to Thetis-faithful
//    Hamming per controller decision 2026-05-10.
//
// 2. specHPSDR.cs:301-322 [v2.10.3.13+501e3f51], DetTypePan / DetTypeWF:
//      private int det_type_pan;     // default 0 (Peak)
//      private int det_type_wf;      // default 0 (Peak)
//
// 3. specHPSDR.cs:382-418 [v2.10.3.13+501e3f51], AverageMode / AverageModeWF:
//      private int av_mode;          // default 0 (None / Off)
//      private int av_mode_wf;       // default 0 (None / Off)
//
// 4. specHPSDR.cs:324-333 [v2.10.3.13+501e3f51], NormOneHzPan:
//      private bool norm_oneHz_pan;  // default false
//
// 5. specHPSDR.cs:351-380 [v2.10.3.13+501e3f51], AvTau / AvTauWF setters
//    use ms-to-seconds (* 0.001) per setup.cs:18125 + :18169:
//      AvTau   = 0.001 * udTXDisplayAVGTime.Value;
//      AvTauWF = 0.001 * udTXDisplayAVTime.Value;
//    Designer-default AvTime is per spinbox:
//      udTXDisplayAVGTime.Value = 30   (Pan AvTime ms; setup.designer.cs:36753)
//      udTXDisplayAVTime.Value  = 120  (WF AvTime ms;  setup.designer.cs:36493)
//
// 6. setup.cs:18112 [v2.10.3.13+501e3f51], normalize gate:
//      // [2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it
//      // only applies to pan detector type 2,3,4
//      chkDispTXNormalize.Enabled = ...DetTypePan >= 2;
//    Detector 2 = Average, 3 = Sample, 4 = RMS per Pan combo ordering.
//
// 7. setup.cs:18136-18143 [v2.10.3.13+501e3f51], FFT size formula:
//      FFTSize = 4096 * Math.Pow(2, slider.Value);
//      bin_width = SampleRate / FFTSize;
//
// 8. specHPSDR.cs:471-480 [v2.10.3.13+501e3f51], _pixel_out default = 2 for
//    pan + waterfall analyzer planes. NereusSDR previously hardcoded
//    n_pixout=1 — 3M-5d bumps to 2 so DetType + AverageMode for pan and WF
//    apply independently (Thetis-faithful).
//
// This test file is NereusSDR-original (test harness, not a Thetis port).

#include <QtTest/QtTest>
#include <QCoreApplication>

#include "core/AppSettings.h"
#include "core/TxAnalyzer.h"

using namespace NereusSDR;

class TestTxAnalyzerSettings : public QObject
{
    Q_OBJECT
private slots:

    void initTestCase()
    {
        // Each test starts with a clean AppSettings slate. The sandbox
        // path is set by TestSandboxInit.cpp before main() runs.
        AppSettings::instance().clear();
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    // 1. defaults_match_spec — TxAnalyzer constructs with Thetis-faithful
    //    defaults: FFT=4096, Window=Hamming (4), PanDet=0, PanAvg=0,
    //    PanAvTimeMs=30, PanNormalize=false, WfDet=0, WfAvg=0,
    //    WfAvTimeMs=120.
    //    Sources (all [v2.10.3.13+501e3f51]):
    //      specHPSDR.cs:134           window_type default 4 (Hamming)
    //      specHPSDR.cs:301, :313     DetTypePan / DetTypeWF default 0
    //      specHPSDR.cs:382, :402     AverageMode / AverageModeWF default 0
    //      specHPSDR.cs:324           NormOneHzPan default false
    //      setup.designer.cs:36753    udTXDisplayAVGTime.Value = 30  (Pan ms)
    //      setup.designer.cs:36493    udTXDisplayAVTime.Value  = 120 (WF ms)
    void defaults_match_spec()
    {
        TxAnalyzer a;

        QCOMPARE(a.fftSize(),         4096);
        QCOMPARE(a.windowType(),      4);     // Hamming
        QCOMPARE(a.panDetector(),     0);     // Peak
        QCOMPARE(a.panAveraging(),    0);     // None / Off
        QCOMPARE(a.panAvTimeMs(),     30);
        QCOMPARE(a.panNormalize(),    false);
        QCOMPARE(a.wfDetector(),      0);     // Peak
        QCOMPARE(a.wfAveraging(),     0);     // None / Off
        QCOMPARE(a.wfAvTimeMs(),      120);
    }

    // 2. settings_round_trip — set each of the 9 properties, save to
    //    AppSettings, construct a fresh TxAnalyzer that loads from
    //    AppSettings, verify each value survives.
    void settings_round_trip()
    {
        {
            TxAnalyzer a;
            a.setFftSize(16384);
            a.setWindowType(1);        // Blackman-Harris 4T
            a.setPanDetector(2);       // Average
            a.setPanAveraging(3);      // Log Recursive
            a.setPanAvTimeMs(75);
            a.setPanNormalize(true);
            a.setWfDetector(3);        // Sample
            a.setWfAveraging(2);       // Time Window
            a.setWfAvTimeMs(250);
            // Setters persist via AppSettings; reconstruction below verifies.
        }

        TxAnalyzer b;  // reads AppSettings on ctor
        QCOMPARE(b.fftSize(),       16384);
        QCOMPARE(b.windowType(),    1);
        QCOMPARE(b.panDetector(),   2);
        QCOMPARE(b.panAveraging(),  3);
        QCOMPARE(b.panAvTimeMs(),   75);
        QCOMPARE(b.panNormalize(),  true);
        QCOMPARE(b.wfDetector(),    3);
        QCOMPARE(b.wfAveraging(),   2);
        QCOMPARE(b.wfAvTimeMs(),    250);
    }

    // 3. fft_size_formula — setFftSizeSliderPosition(N) yields fftSize =
    //    4096 * 2^N. Mirrors Thetis tbTXDisplayFFTSize_Scroll at
    //    setup.cs:18136-18143 [v2.10.3.13+501e3f51]:
    //      FFTSize = (int)(4096 * Math.Pow(2, slider.Value));
    void fft_size_formula()
    {
        TxAnalyzer a;
        a.setFftSizeSliderPosition(0);
        QCOMPARE(a.fftSize(), 4096);
        a.setFftSizeSliderPosition(1);
        QCOMPARE(a.fftSize(), 8192);
        a.setFftSizeSliderPosition(2);
        QCOMPARE(a.fftSize(), 16384);
        a.setFftSizeSliderPosition(3);
        QCOMPARE(a.fftSize(), 32768);
    }

    // 4. bin_width_formula — binWidthHz() == sampleRate / fftSize. Mirrors
    //    setup.cs:18140 [v2.10.3.13+501e3f51]:
    //      bin_width = (double)SampleRate / (double)FFTSize;
    void bin_width_formula()
    {
        TxAnalyzer a;
        // Default 96 kHz / 4096 = 23.4375 Hz
        QCOMPARE(a.binWidthHz(), 96000.0 / 4096.0);
        a.setFftSizeSliderPosition(2);  // 16384
        QCOMPARE(a.binWidthHz(), 96000.0 / 16384.0);
    }

    // 5. normalize_gated_on_pan_detector — panNormalizeEnabled() is false
    //    when DetTypePan < 2; true when DetTypePan >= 2. Mirrors Thetis
    //    setup.cs:18111-18112 [v2.10.3.13+501e3f51]:
    //      //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR
    //      // as it only applies to pan detector type 2,3,4
    //      chkDispTXNormalize.Enabled = ...DetTypePan >= 2;
    void normalize_gated_on_pan_detector()
    {
        TxAnalyzer a;
        a.setPanDetector(0);   // Peak
        QCOMPARE(a.panNormalizeEnabled(), false);
        a.setPanDetector(1);   // Rosenfell
        QCOMPARE(a.panNormalizeEnabled(), false);
        a.setPanDetector(2);   // Average
        QCOMPARE(a.panNormalizeEnabled(), true);
        a.setPanDetector(3);   // Sample
        QCOMPARE(a.panNormalizeEnabled(), true);
        a.setPanDetector(4);   // RMS
        QCOMPARE(a.panNormalizeEnabled(), true);
    }

    // 6. av_time_ms_to_tau_seconds — setter accepts ms; tau() returns
    //    seconds (= ms * 0.001). Mirrors Thetis setup.cs:18125 + :18169
    //    [v2.10.3.13+501e3f51]:
    //      AvTau   = 0.001 * (double)udTXDisplayAVGTime.Value;   (pan)
    //      AvTauWF = 0.001 * (double)udTXDisplayAVTime.Value;    (wf)
    void av_time_ms_to_tau_seconds()
    {
        TxAnalyzer a;
        a.setPanAvTimeMs(100);
        QCOMPARE(a.panAvTauSeconds(), 0.100);
        a.setWfAvTimeMs(250);
        QCOMPARE(a.wfAvTauSeconds(), 0.250);
    }

    // 7. setAnalyzer_called_on_each_setter — every setter on the 9-control
    //    state surface triggers at least one analyzer-side reconfiguration
    //    (SetAnalyzer for full re-init, or SetDisplayDetectorMode /
    //    SetDisplayAverageMode / SetDisplayAvBackmult /
    //    SetDisplayNormOneHz for the finer-grained ones). The
    //    analyzerConfigCount() test seam ticks every time TxAnalyzer
    //    pushes any config to WDSP.
    void setAnalyzer_called_on_each_setter()
    {
        TxAnalyzer a;
        const int base = a.analyzerConfigCount();

        a.setFftSize(8192);
        QVERIFY(a.analyzerConfigCount() > base);

        const int n1 = a.analyzerConfigCount();
        a.setWindowType(1);
        QVERIFY(a.analyzerConfigCount() > n1);

        const int n2 = a.analyzerConfigCount();
        a.setPanDetector(2);
        QVERIFY(a.analyzerConfigCount() > n2);

        const int n3 = a.analyzerConfigCount();
        a.setPanAveraging(1);
        QVERIFY(a.analyzerConfigCount() > n3);

        const int n4 = a.analyzerConfigCount();
        a.setPanAvTimeMs(80);
        QVERIFY(a.analyzerConfigCount() > n4);

        const int n5 = a.analyzerConfigCount();
        a.setPanNormalize(true);
        QVERIFY(a.analyzerConfigCount() > n5);

        const int n6 = a.analyzerConfigCount();
        a.setWfDetector(2);
        QVERIFY(a.analyzerConfigCount() > n6);

        const int n7 = a.analyzerConfigCount();
        a.setWfAveraging(1);
        QVERIFY(a.analyzerConfigCount() > n7);

        const int n8 = a.analyzerConfigCount();
        a.setWfAvTimeMs(200);
        QVERIFY(a.analyzerConfigCount() > n8);
    }

    // 8. pixout_2_after_phase — n_pixout() reads 2 so pan (pixout 0) and
    //    waterfall (pixout 1) get independent DetType + AverageMode
    //    applied by WDSP. 3M-5b shipped n_pixout=1 (pan and wf shared one
    //    pixel-out). 3M-5d bumps to 2 to match Thetis specHPSDR.cs:471-480
    //    [v2.10.3.13+501e3f51] where _pixel_out defaults to 2.
    void pixout_2_after_phase()
    {
        TxAnalyzer a;
        QCOMPARE(a.nPixout(), 2);
    }
};

QTEST_MAIN(TestTxAnalyzerSettings)
#include "tst_tx_analyzer_settings.moc"
