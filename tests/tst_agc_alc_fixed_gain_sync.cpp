// no-port-check: NereusSDR-original unit-test file; no Thetis logic ported.
// =================================================================
// tests/tst_agc_alc_fixed_gain_sync.cpp  (NereusSDR)
// =================================================================
//
// Settings > DSP > AGC/ALC: the Fixed Gain and Max Gain spinners follow
// the slice. A
// write from elsewhere (a remote window, the phone, a future AGC-T in Off)
// shows on an open page, and the update does not echo back into the model.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-15 - New test file: model-to-spinner sync for Fixed Gain.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-10-15 - Max Gain spinner sync and the rebind case. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-22 - The RX Leveler's "Gain now" line: off after RXL Off, the
//                 voice-modes note in a mode RXL leaves alone, the gain in a
//                 voice mode. J.J. Boyd (KG4VCF), AI-assisted via Claude
//                 Code.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QSpinBox>

#include "gui/setup/DspSetupPages.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

QSpinBox* spinnerById(QWidget* page, const char* id)
{
    for (QSpinBox* spin : page->findChildren<QSpinBox*>()) {
        if (spin->property("nereusSetupId").toString() == QLatin1String(id)) {
            return spin;
        }
    }
    return nullptr;
}

} // namespace

class TestAgcAlcFixedGainSync : public QObject {
    Q_OBJECT

private slots:
    // "Gain now" follows the slice's RXL mode and receive mode, not only its
    // gain: off after RXL Off and in a voice mode RXL leaves alone the
    // phone's words, even when the gain reading does not move.
    void rxLevelerGainNowShowsOffAndTheModeNote()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        slice->setRxLevelerMode(RxLevelerMode::Auto);
        AgcAlcSetupPage page(&model);
        QLabel* line = nullptr;
        for (QLabel* label : page.findChildren<QLabel*>()) {
            if (label->property("nereusSetupId").toString()
                == QLatin1String("dsp.agcAlc.rxLevelerBoost")) {
                line = label;
            }
        }
        QVERIFY(line != nullptr);
        QCOMPARE(line->text(), QStringLiteral("Gain now: 0.0 dB"));
        slice->setRxLevelerGainDb(3.24);
        QCOMPARE(line->text(), QStringLiteral("Gain now: +3.2 dB"));
        slice->setRxLevelerGainDb(0.0);

        slice->setRxLevelerMode(RxLevelerMode::Off);
        QCOMPARE(line->text(), QStringLiteral("Gain now: off"));
        slice->setRxLevelerMode(RxLevelerMode::Auto);
        QCOMPARE(line->text(), QStringLiteral("Gain now: 0.0 dB"));

        slice->setDspMode(DSPMode::CWU);
        QCOMPARE(line->text(), QStringLiteral(
            "RXL levels voice modes (LSB, USB, DSB, AM, SAM). In this mode the audio "
            "is left as it is."));
        slice->setDspMode(DSPMode::LSB);
        QCOMPARE(line->text(), QStringLiteral("Gain now: 0.0 dB"));
    }

    void modelChangeUpdatesSpinnerWithoutEcho()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);

        AgcAlcSetupPage page(&model);
        QSpinBox* spin = spinnerById(&page, "dsp.agcAlc.agcFixedGain");
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), slice->agcFixedGain());

        QSignalSpy modelSpy(slice, &SliceModel::agcFixedGainChanged);
        QSignalSpy spinSpy(spin, QOverload<int>::of(&QSpinBox::valueChanged));

        slice->setAgcFixedGain(45);
        QCOMPARE(spin->value(), 45);
        // One model emit, none from the spinner: no echo back.
        QCOMPARE(modelSpy.count(), 1);
        QCOMPARE(spinSpy.count(), 0);

        // The spinner still writes the model.
        spin->setValue(30);
        QCOMPARE(slice->agcFixedGain(), 30);
    }

    void maxGainModelChangeUpdatesSpinnerWithoutEcho()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);

        AgcAlcSetupPage page(&model);
        QSpinBox* spin = spinnerById(&page, "dsp.agcAlc.agcMaxGain");
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), slice->agcMaxGain());

        QSignalSpy modelSpy(slice, &SliceModel::agcMaxGainChanged);
        QSignalSpy spinSpy(spin, QOverload<int>::of(&QSpinBox::valueChanged));

        slice->setAgcMaxGain(70);
        QCOMPARE(spin->value(), 70);
        QCOMPARE(modelSpy.count(), 1);
        QCOMPARE(spinSpy.count(), 0);

        spin->setValue(100);
        QCOMPARE(slice->agcMaxGain(), 100);
    }

    // After the page rebinds to another slice, the old slice no longer
    // drives the new spinners.
    void rebindDropsTheOldSlice()
    {
        RadioModel model;
        const int a = model.addSlice();
        const int b = model.addSlice();
        model.setActiveSlice(a);
        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceA != nullptr && sliceB != nullptr);

        AgcAlcSetupPage page(&model);
        model.setActiveSlice(b);
        QCOMPARE(model.activeSlice(), sliceB);

        QSpinBox* fixedSpin = spinnerById(&page, "dsp.agcAlc.agcFixedGain");
        QSpinBox* maxSpin = spinnerById(&page, "dsp.agcAlc.agcMaxGain");
        QVERIFY(fixedSpin != nullptr && maxSpin != nullptr);
        QCOMPARE(fixedSpin->value(), sliceB->agcFixedGain());
        QCOMPARE(maxSpin->value(), sliceB->agcMaxGain());

        const int fixedB = sliceB->agcFixedGain();
        const int maxB = sliceB->agcMaxGain();
        sliceA->setAgcFixedGain(fixedB + 7);
        sliceA->setAgcMaxGain(maxB - 7);
        QCOMPARE(fixedSpin->value(), fixedB);
        QCOMPARE(maxSpin->value(), maxB);

        // The new slice does drive them.
        sliceB->setAgcFixedGain(fixedB + 3);
        sliceB->setAgcMaxGain(maxB - 3);
        QCOMPARE(fixedSpin->value(), fixedB + 3);
        QCOMPARE(maxSpin->value(), maxB - 3);
    }
};

QTEST_MAIN(TestAgcAlcFixedGainSync)
#include "tst_agc_alc_fixed_gain_sync.moc"
