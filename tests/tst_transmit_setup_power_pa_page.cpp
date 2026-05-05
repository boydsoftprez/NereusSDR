// no-port-check: unit tests for PowerPage H.4 wiring (3M-1a).
//
// Note: filename retains 'power_pa_page' for git history continuity.
// Class renamed to PowerPage in IA reshape Phase 6 (2026-05-02).
//
// Tests verify:
//   1. Max Power slider value initialises from TransmitModel::power().
//   2. Changing the slider updates TransmitModel::power().
//   3. Changing TransmitModel::power() updates the slider (reverse).
//   4. chkATTOnTX initialises from StepAttenuatorController::attOnTxEnabled().
//   5. chkATTOnTX toggle updates StepAttenuatorController::attOnTxEnabled().
//   6. chkForceATTwhenPSAoff initialises from StepAttenuatorController::forceAttWhenPsOff().
//   7. chkForceATTwhenPSAoff toggle updates StepAttenuatorController::forceAttWhenPsOff().
//
// Thetis references:
//   Max Power slider — console.cs:4822 [v2.10.3.13] PWR setter
//   chkATTOnTX — setup.designer.cs:5926-5939 [v2.10.3.13] + setup.cs:15452-15455
//   chkForceATTwhenPSAoff — setup.designer.cs:5660-5671 [v2.10.3.13] + setup.cs:24264-24268
//   //MW0LGE [2.9.0.7] added  [original inline comment from console.cs:29285]
//
// Issue #175 Wave 1: per-band Tune Power 14-spinbox grid dropped (no Thetis
// upstream — Thetis uses a single udTXTunePower with active-band pivot).
// Tune-power test cases below removed; the model-level coverage in
// tst_transmit_model_tune_power.cpp stays untouched.

#include <QtTest/QtTest>
#include <QApplication>
#include <QSlider>
#include <QCheckBox>
#include <QSignalSpy>

#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/StepAttenuatorController.h"

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// TestPowerPageH4 — PowerPage wiring verification for 3M-1a H.4.
// ---------------------------------------------------------------------------
class TestPowerPageH4 : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void maxPowerSlider_initFromModel();
    void maxPowerSlider_changesModel();
    void maxPowerSlider_updatesFromModel();

    void chkAttOnTx_initFromController();
    void chkAttOnTx_togglesController();
    // Wired-controller variants: verify actual call-through when controller present.
    void chkAttOnTx_wiredController_initFromController();
    void chkAttOnTx_wiredController_togglesController();

    void chkForceAttWhenPsOff_initFromController();
    void chkForceAttWhenPsOff_togglesController();
    // Wired-controller variants: verify actual call-through when controller present.
    void chkForceAttWhenPsOff_wiredController_initFromController();
    void chkForceAttWhenPsOff_wiredController_togglesController();
};

void TestPowerPageH4::initTestCase()
{
    if (!qApp) {
        static int   argc = 0;
        static char* argv = nullptr;
        new QApplication(argc, &argv);
    }
}

// ---------------------------------------------------------------------------
// 1. Max Power slider initialises from TransmitModel::power()
// ---------------------------------------------------------------------------
void TestPowerPageH4::maxPowerSlider_initFromModel()
{
    RadioModel model;
    model.transmitModel().setPower(42);

    PowerPage page(&model);
    page.show();  // realize widgets

    auto* slider = page.findChild<QSlider*>(QStringLiteral("maxPowerSlider"));
    QVERIFY(slider);
    QCOMPARE(slider->value(), 42);
}

// ---------------------------------------------------------------------------
// 2. Changing the Max Power slider updates TransmitModel::power()
// ---------------------------------------------------------------------------
void TestPowerPageH4::maxPowerSlider_changesModel()
{
    RadioModel model;
    PowerPage page(&model);
    page.show();

    auto* slider = page.findChild<QSlider*>(QStringLiteral("maxPowerSlider"));
    QVERIFY(slider);

    slider->setValue(73);
    QCOMPARE(model.transmitModel().power(), 73);
}

// ---------------------------------------------------------------------------
// 3. Changing TransmitModel::power() updates the slider (reverse wiring)
// ---------------------------------------------------------------------------
void TestPowerPageH4::maxPowerSlider_updatesFromModel()
{
    RadioModel model;
    PowerPage page(&model);
    page.show();

    auto* slider = page.findChild<QSlider*>(QStringLiteral("maxPowerSlider"));
    QVERIFY(slider);

    model.transmitModel().setPower(55);
    QCOMPARE(slider->value(), 55);
}

// ---------------------------------------------------------------------------
// 4. chkATTOnTX initialises from StepAttenuatorController::attOnTxEnabled()
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkAttOnTx_initFromController()
{
    RadioModel model;

    // StepAttenuatorController defaults to attOnTxEnabled = true (console.cs:19042 default).
    // Confirm the checkbox reflects this.
    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkATTOnTX"));
    // If there's no StepAttenuatorController in the model (headless), the
    // checkbox may not be found via the model path — but it must still exist.
    // The test verifies the widget exists; state verification only when controller
    // is wired.
    QVERIFY(chk);
}

// ---------------------------------------------------------------------------
// 8. chkATTOnTX toggle updates StepAttenuatorController::attOnTxEnabled()
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkAttOnTx_togglesController()
{
    RadioModel model;
    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkATTOnTX"));
    QVERIFY(chk);

    // When StepAttenuatorController is null (headless RadioModel), toggle
    // must not crash (the connect is guarded by if (att)).
    const bool before = chk->isChecked();
    chk->setChecked(!before);   // toggle
    // No crash = guard works.
}

// ---------------------------------------------------------------------------
// 9. chkForceATTwhenPSAoff initialises from StepAttenuatorController::forceAttWhenPsOff()
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkForceAttWhenPsOff_initFromController()
{
    RadioModel model;
    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkForceATTwhenPSAoff"));
    QVERIFY(chk);
}

// ---------------------------------------------------------------------------
// 10. chkForceATTwhenPSAoff toggle no-crash when controller is null
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkForceAttWhenPsOff_togglesController()
{
    RadioModel model;
    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkForceATTwhenPSAoff"));
    QVERIFY(chk);

    const bool before = chk->isChecked();
    chk->setChecked(!before);   // toggle — must not crash
}

// ---------------------------------------------------------------------------
// 11. chkATTOnTX initialises from controller when controller is wired
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkAttOnTx_wiredController_initFromController()
{
    RadioModel model;
    // Wire a real StepAttenuatorController so the checkbox reads from it.
    // Thetis default is attOnTxEnabled = true (console.cs:19042 [v2.10.3.13]).
    auto* att = new StepAttenuatorController(&model);
    att->setAttOnTxEnabled(false);          // set to non-default so the init is observable
    model.setStepAttController(att);

    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkATTOnTX"));
    QVERIFY(chk);
    QCOMPARE(chk->isChecked(), false);      // must mirror the controller value

    att->setAttOnTxEnabled(true);
    // No toggle signal path from controller → page; the initial state is all we verify here.
    // The bidirectional path is: checkbox → controller (tested below).
}

// ---------------------------------------------------------------------------
// 12. chkATTOnTX toggle actually calls StepAttenuatorController::setAttOnTxEnabled()
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkAttOnTx_wiredController_togglesController()
{
    RadioModel model;
    auto* att = new StepAttenuatorController(&model);
    att->setAttOnTxEnabled(true);           // start checked
    model.setStepAttController(att);

    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkATTOnTX"));
    QVERIFY(chk);
    QVERIFY(chk->isChecked());              // sanity: initialised from controller

    // Toggle the checkbox — must propagate to the controller.
    chk->setChecked(false);
    QCOMPARE(att->attOnTxEnabled(), false);

    chk->setChecked(true);
    QCOMPARE(att->attOnTxEnabled(), true);
}

// ---------------------------------------------------------------------------
// 13. chkForceATTwhenPSAoff initialises from controller when controller is wired
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkForceAttWhenPsOff_wiredController_initFromController()
{
    RadioModel model;
    auto* att = new StepAttenuatorController(&model);
    // Thetis default is forceAttWhenPsOff = true (setup.cs:24264 [v2.10.3.13]).
    att->setForceAttWhenPsOff(false);       // non-default so init is observable
    model.setStepAttController(att);

    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkForceATTwhenPSAoff"));
    QVERIFY(chk);
    QCOMPARE(chk->isChecked(), false);
}

// ---------------------------------------------------------------------------
// 14. chkForceATTwhenPSAoff toggle actually calls StepAttenuatorController::setForceAttWhenPsOff()
// ---------------------------------------------------------------------------
void TestPowerPageH4::chkForceAttWhenPsOff_wiredController_togglesController()
{
    RadioModel model;
    auto* att = new StepAttenuatorController(&model);
    att->setForceAttWhenPsOff(true);        // start checked
    model.setStepAttController(att);

    PowerPage page(&model);
    page.show();

    auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkForceATTwhenPSAoff"));
    QVERIFY(chk);
    QVERIFY(chk->isChecked());              // sanity: initialised from controller

    // Toggle the checkbox — must propagate to the controller.
    chk->setChecked(false);
    QCOMPARE(att->forceAttWhenPsOff(), false);

    chk->setChecked(true);
    QCOMPARE(att->forceAttWhenPsOff(), true);
}

QTEST_MAIN(TestPowerPageH4)
#include "tst_transmit_setup_power_pa_page.moc"
