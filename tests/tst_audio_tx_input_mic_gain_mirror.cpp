// tests/tst_audio_tx_input_mic_gain_mirror.cpp  (NereusSDR)
//
// Phase 3M-1b Task I.4 — Per-board mic gain slider range on AudioTxInputPage.
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies:
//   1.  Unknown board: slider min = -50, max = +70 (TransmitModel fallback range).
//   2.  Known board (Hermes): slider min = -40, max = +10 (Thetis runtime defaults).
//   3.  Known board (Saturn): slider min = -40, max = +10 (same Thetis defaults).
//   4.  Known board (HermesLite): slider min = -40, max = +10 (hasMicJack=false
//       but BoardCapabilities still stores the correct range).
//   5.  Slider initial value clamped to range: if model holds -50 dB and board
//       range is -40..+10, slider must be at -40 (clamped to minimum).
//   6.  Slider initial value clamped high: model = +70 dB, Hermes range → +10.
//   7.  Model bidirectional — UI → model: move slider to +5 → micGainDb = +5.
//   8.  Model bidirectional — model → UI: setMicGainDb(-20) → slider at -20.
//   9.  setMicGainDb(value) above slider max is clamped by TransmitModel before
//       reaching the slider (TransmitModel::setMicGainDb clamps to kMicGainDb*).
//  10.  No feedback loop: model → UI → no re-emission (QSignalSpy count = 1).
//
// TxApplet mirror test:
//   The bidirectional mirror across AudioTxInputPage ↔ TxApplet is deferred
//   to Phase 3M-1b Task J.1, when TxApplet gains a Mic Gain slider.  Tests
//   7 and 8 above exercise the integration point (TransmitModel signals) that
//   the J.1 TxApplet slider will share.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSlider>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "gui/setup/AudioTxInputPage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestAudioTxInputMicGainMirror : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    // ── 1. Unknown board: slider range = -50..+70 ─────────────────────────────

    void unknownBoard_sliderRange_fallback()
    {
        // Default RadioModel (no test-cap override) returns Unknown board.
        RadioModel model;
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");
        QCOMPARE(slider->minimum(), -50);
        QCOMPARE(slider->maximum(), +70);
    }

    // ── 2. Hermes board: slider range = -40..+10 ──────────────────────────────

    void hermesBoard_sliderRange_thetisDefaults()
    {
        // From Thetis console.cs:19151-19171 [v2.10.3.13]:
        //   mic_gain_min = -40, mic_gain_max = 10
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");
        QCOMPARE(slider->minimum(), -40);
        QCOMPARE(slider->maximum(), +10);
    }

    // ── 3. Saturn board: slider range = -40..+10 ──────────────────────────────

    void saturnBoard_sliderRange_thetisDefaults()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");
        QCOMPARE(slider->minimum(), -40);
        QCOMPARE(slider->maximum(), +10);
    }

    // ── 4. HermesLite board: BoardCapabilities still stores -40/+10 ───────────
    // HL2 has no mic jack (hasMicJack=false) so the slider is still built
    // in the PC Mic group with the canonical board range.

    void hermesLiteBoard_sliderRange_thetisDefaults()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::HermesLite);
        model.setCapsHasMicJackForTest(false);
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");
        QCOMPARE(slider->minimum(), -40);
        QCOMPARE(slider->maximum(), +10);
    }

    // ── 5. Slider clamps low: model = -50 dB, Hermes range → slider at -40 ───

    void slider_clampsToMinimum_whenModelValueBelowRange()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Hermes);

        // Set the model mic gain below the board minimum (-40).
        model.transmitModel().setMicGainDb(-50);

        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");
        // Slider must be clamped to the board minimum, not -50.
        QCOMPARE(slider->value(), slider->minimum());
        QCOMPARE(slider->value(), -40);
    }

    // ── 6. Slider clamps high: model = +70 dB, Hermes range → slider at +10 ──

    void slider_clampsToMaximum_whenModelValueAboveRange()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Hermes);

        // Pump a high value into TransmitModel via the global kMicGainDbMax.
        // TransmitModel clamps to kMicGainDbMax = +70; the slider then
        // clamps further to the board maximum (+10).
        model.transmitModel().setMicGainDb(+70);

        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");
        QCOMPARE(slider->value(), slider->maximum());
        QCOMPARE(slider->value(), +10);
    }

    // ── 7. UI → model: move slider to +5 → micGainDb = +5 ────────────────────

    void sliderUI_setsModelMicGainDb()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");

        // Set slider value programmatically (emulates user drag).
        slider->setValue(5);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().micGainDb(), 5);
    }

    // ── 8. Model → UI: setMicGainDb(-20) → slider at -20 ────────────────────

    void modelChange_updatesMicGainSlider()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Hermes);
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");

        model.transmitModel().setMicGainDb(-20);
        QApplication::processEvents();

        QCOMPARE(slider->value(), -20);
    }

    // ── 9. TransmitModel::setMicGainDb clamps above kMicGainDbMax ────────────

    void transmitModel_clampsAboveGlobalMax()
    {
        RadioModel model;
        // Unknown board → slider max = +70 = kMicGainDbMax.
        AudioTxInputPage page(&model);

        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");

        // Attempt to set above the global maximum.
        model.transmitModel().setMicGainDb(+999);
        QApplication::processEvents();

        // TransmitModel clamps to kMicGainDbMax before emitting the signal.
        QCOMPARE(model.transmitModel().micGainDb(), TransmitModel::kMicGainDbMax);
        QCOMPARE(slider->value(), TransmitModel::kMicGainDbMax);
    }

    // ── 10. No feedback loop: model → UI → no re-emission ────────────────────

    void noFeedbackLoop_modelToUi()
    {
        RadioModel model;
        model.setCapsHwForTest(HPSDRHW::Saturn);
        AudioTxInputPage page(&model);

        QSignalSpy spy(&model.transmitModel(), &TransmitModel::micGainDbChanged);

        // Trigger via model setter — should fire exactly once (model → UI)
        // and the UI → model path must NOT re-fire the signal.
        model.transmitModel().setMicGainDb(0);
        QApplication::processEvents();

        QCOMPARE(spy.count(), 1);
    }

};

QTEST_MAIN(TestAudioTxInputMicGainMirror)
#include "tst_audio_tx_input_mic_gain_mirror.moc"
