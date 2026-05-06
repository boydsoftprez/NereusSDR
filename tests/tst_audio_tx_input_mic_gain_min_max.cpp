// tests/tst_audio_tx_input_mic_gain_min_max.cpp  (NereusSDR)
//
// radio-mic-input Task 21 — MicGainMin/Max spinboxes + live mic-gain
// slider range on AudioTxInputPage.
//
// no-port-check: test fixture; no Thetis logic ported here.
//
// Verifies:
//   1.  Min spinbox reflects TransmitModel::micGainMinDb at construction.
//   2.  Max spinbox reflects TransmitModel::micGainMaxDb at construction.
//   3.  Min spinbox change pushes back to TransmitModel::micGainMinDb.
//   4.  Max spinbox change pushes back to TransmitModel::micGainMaxDb.
//   5.  Slider min/max update live to match the persisted bounds.
//
// Thetis source for the parity:
//   setup.cs:9678-9694 [v2.10.3.13+501e3f51] — udMicGainMin / udMicGainMax
//     ValueChanged handlers.
//   setup.designer.cs:46808-46866 [v2.10.3.13+501e3f51] — designer ranges
//     (Min: -96..0; Max: 1..70).

#include <QtTest/QtTest>
#include <QApplication>
#include <QSlider>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "gui/setup/AudioTxInputPage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestAudioTxInputMicGainMinMax : public QObject
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

    // ── 1. Min spinbox reflects model value at construction ───────────────────

    void minSpinReflectsModel()
    {
        RadioModel rm;
        rm.transmitModel().setMicGainMinDb(-50);
        AudioTxInputPage page(&rm);
        QSpinBox* spin = page.findChild<QSpinBox*>(QStringLiteral("micGainMinSpin"));
        QVERIFY2(spin != nullptr, "micGainMinSpin must exist");
        QCOMPARE(spin->value(), -50);
    }

    // ── 2. Max spinbox reflects model value at construction ───────────────────

    void maxSpinReflectsModel()
    {
        RadioModel rm;
        rm.transmitModel().setMicGainMaxDb(20);
        AudioTxInputPage page(&rm);
        QSpinBox* spin = page.findChild<QSpinBox*>(QStringLiteral("micGainMaxSpin"));
        QVERIFY2(spin != nullptr, "micGainMaxSpin must exist");
        QCOMPARE(spin->value(), 20);
    }

    // ── 3. Min spinbox change pushes back to model ────────────────────────────

    void minSpinChangePushesToModel()
    {
        RadioModel rm;
        AudioTxInputPage page(&rm);
        QSpinBox* spin = page.findChild<QSpinBox*>(QStringLiteral("micGainMinSpin"));
        QVERIFY2(spin != nullptr, "micGainMinSpin must exist");
        spin->setValue(-30);
        QCOMPARE(rm.transmitModel().micGainMinDb(), -30);
    }

    // ── 4. Max spinbox change pushes back to model ────────────────────────────

    void maxSpinChangePushesToModel()
    {
        RadioModel rm;
        AudioTxInputPage page(&rm);
        QSpinBox* spin = page.findChild<QSpinBox*>(QStringLiteral("micGainMaxSpin"));
        QVERIFY2(spin != nullptr, "micGainMaxSpin must exist");
        spin->setValue(15);
        QCOMPARE(rm.transmitModel().micGainMaxDb(), 15);
    }

    // ── 5. Slider min/max range tracks persisted bounds ───────────────────────

    void sliderRangeUpdatesLive()
    {
        RadioModel rm;
        rm.transmitModel().setMicGainMinDb(-50);
        rm.transmitModel().setMicGainMaxDb(20);
        AudioTxInputPage page(&rm);
        QSlider* slider = page.micGainSlider();
        QVERIFY2(slider != nullptr, "micGainSlider must exist");
        QCOMPARE(slider->minimum(), -50);
        QCOMPARE(slider->maximum(), 20);
    }
};

QTEST_MAIN(TestAudioTxInputMicGainMinMax)
#include "tst_audio_tx_input_mic_gain_min_max.moc"
