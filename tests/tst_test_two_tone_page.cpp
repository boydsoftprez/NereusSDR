// tests/tst_test_two_tone_page.cpp  (NereusSDR)
//
// Phase 3M-1c Task H — Setup → Test → Two-Tone page
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies:
//   1.  Constructor doesn't crash with a default RadioModel.
//   2.  Initial widget values match TransmitModel defaults (8 properties).
//   3.  UI → Model: spinbox setValue drives TransmitModel::setTwoToneFreq1.
//   3b. UI → Model: levelSpin → setTwoToneLevel.
//   3c. UI → Model: powerSpin → setTwoTonePower.
//   3d. UI → Model: delaySpin → setTwoToneFreq2Delay.
//   3e. UI → Model: invertChk → setTwoToneInvert.
//   3f. UI → Model: pulsedChk → setTwoTonePulsed.
//   4.  Model → UI: setTwoToneFreq1(800) drives QSpinBox value.
//   4b. Model → UI: setTwoToneLevel(-12) drives the QDoubleSpinBox.
//   5.  Defaults preset button: click → Freq1=700 / Freq2=1900; level/power
//       unchanged (matches Thetis btnTwoToneF_defaults_Click semantics).
//   6.  Stealth preset button: click → Freq1=70 / Freq2=190.
//   7.  Invert checkbox initial state is checked (default twoToneInvert=true).
//   8.  Pulsed checkbox initial state is unchecked.
//   9.  DrivePowerSource radio buttons: setTwoToneDrivePowerSource(Fixed)
//       selects "Fixed" radio button; clicking "Tune Slider" drives the model.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "gui/setup/TestTwoTonePage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTestTwoTonePage : public QObject
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

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. Constructor doesn't crash with a default RadioModel ────────────────

    void constructor_doesNotCrash()
    {
        RadioModel model;
        TestTwoTonePage page(&model);
        QVERIFY(true);  // Survived construction.
    }

    // ── 2. Initial widget values match TransmitModel defaults ─────────────────

    void initialValues_matchDefaults()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        QCOMPARE(page.freq1Spin()->value(), 700);
        QCOMPARE(page.freq2Spin()->value(), 1900);
        QCOMPARE(page.levelSpin()->value(), 0.0);  // Phase 3M-4 Task 17
        QCOMPARE(page.powerSpin()->value(), 10);  // Thetis-faithful default (2026-05-23)
        QCOMPARE(page.freq2DelaySpin()->value(), 0);
        QCOMPARE(page.invertCheck()->isChecked(), true);
        QCOMPARE(page.pulsedCheck()->isChecked(), false);
        QCOMPARE(page.driveSliderRadio()->isChecked(), true);
        QCOMPARE(page.tuneSliderRadio()->isChecked(), false);
        QCOMPARE(page.fixedDriveRadio()->isChecked(), false);
    }

    // ── 3. UI → Model round trips ─────────────────────────────────────────────

    void uiToModel_freq1Spin()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.freq1Spin()->setValue(1234);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneFreq1(), 1234);
    }

    void uiToModel_freq2Spin()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.freq2Spin()->setValue(2345);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneFreq2(), 2345);
    }

    void uiToModel_levelSpin()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.levelSpin()->setValue(-12.5);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneLevel(), -12.5);
    }

    void uiToModel_powerSpin()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.powerSpin()->setValue(75);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoTonePower(), 75);
    }

    void uiToModel_freq2DelaySpin()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.freq2DelaySpin()->setValue(250);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneFreq2Delay(), 250);
    }

    void uiToModel_invertCheck()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        // Default is true; uncheck it.
        page.invertCheck()->setChecked(false);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneInvert(), false);
    }

    void uiToModel_pulsedCheck()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.pulsedCheck()->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoTonePulsed(), true);
    }

    // ── 4. Model → UI round trips ─────────────────────────────────────────────

    void modelToUi_freq1Spin()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        model.transmitModel().setTwoToneFreq1(800);
        QApplication::processEvents();

        QCOMPARE(page.freq1Spin()->value(), 800);
    }

    void modelToUi_levelSpin()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        model.transmitModel().setTwoToneLevel(-12.0);
        QApplication::processEvents();

        QCOMPARE(page.levelSpin()->value(), -12.0);
    }

    // ── 5. Defaults preset button ─────────────────────────────────────────────

    void defaultsButton_setsFreqsOnly()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        // Set non-default values for everything first.
        model.transmitModel().setTwoToneFreq1(1234);
        model.transmitModel().setTwoToneFreq2(5678);
        model.transmitModel().setTwoToneLevel(-30.0);
        model.transmitModel().setTwoTonePower(20);
        QApplication::processEvents();

        // Click Defaults: should reset Freq1 and Freq2 ONLY.
        QTest::mouseClick(page.defaultsButton(), Qt::LeftButton);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneFreq1(), 700);
        QCOMPARE(model.transmitModel().twoToneFreq2(), 1900);
        // Level and Power must NOT be touched by the preset.
        QCOMPARE(model.transmitModel().twoToneLevel(), -30.0);
        QCOMPARE(model.transmitModel().twoTonePower(), 20);
    }

    // ── 6. Stealth preset button ──────────────────────────────────────────────

    void stealthButton_setsFreqsOnly()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        QTest::mouseClick(page.stealthButton(), Qt::LeftButton);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneFreq1(), 70);
        QCOMPARE(model.transmitModel().twoToneFreq2(), 190);
    }

    // ── 7. Invert checkbox initial state checked (default true) ───────────────

    void invertCheckbox_defaultChecked()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        QVERIFY2(page.invertCheck()->isChecked(),
                 "twoToneInvert default true → invert checkbox must be checked");
    }

    // ── 8. Pulsed checkbox initial state unchecked ────────────────────────────

    void pulsedCheckbox_defaultUnchecked()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        QVERIFY2(!page.pulsedCheck()->isChecked(),
                 "twoTonePulsed default false → pulsed checkbox must be unchecked");
    }

    // ── 9. DrivePowerSource radio button ↔ model two-way sync ────────────────

    void modelToUi_drivePowerFixed_selectsFixedRadio()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        model.transmitModel().setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        QApplication::processEvents();

        QVERIFY2(page.fixedDriveRadio()->isChecked(),
                 "Fixed model state must select 'Fixed' radio button");
        QVERIFY2(!page.driveSliderRadio()->isChecked(),
                 "DriveSlider radio must be unchecked");
        QVERIFY2(!page.tuneSliderRadio()->isChecked(),
                 "TuneSlider radio must be unchecked");
    }

    void uiToModel_clickTuneSlider_setsModel()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.tuneSliderRadio()->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneDrivePowerSource(),
                 DrivePowerSource::TuneSlider);
    }

    void uiToModel_clickFixed_setsModel()
    {
        RadioModel model;
        TestTwoTonePage page(&model);

        page.fixedDriveRadio()->setChecked(true);
        QApplication::processEvents();

        QCOMPARE(model.transmitModel().twoToneDrivePowerSource(),
                 DrivePowerSource::Fixed);
    }
};

QTEST_APPLESS_MAIN(TestTestTwoTonePage)
#include "tst_test_two_tone_page.moc"
