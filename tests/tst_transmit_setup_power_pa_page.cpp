// no-port-check: unit tests for PowerPage H.4 wiring (3M-1a).
//
// Note: filename retains 'power_pa_page' for git history continuity.
// Class renamed to PowerPage in IA reshape Phase 6 (2026-05-02).
//
// Tests verify:
//   1. Max Power slider value initialises from TransmitModel::power().
//   2. Changing the slider updates TransmitModel::power().
//   3. Changing TransmitModel::power() updates the slider (reverse).
//   4. Per-band tune-power spinboxes initialise from TransmitModel.
//   5. Editing a spinbox updates TransmitModel::tunePowerForBand().
//   6. TransmitModel::tunePowerByBandChanged updates the spinbox (reverse).
//   7. chkATTOnTX initialises from StepAttenuatorController::attOnTxEnabled().
//   8. chkATTOnTX toggle updates StepAttenuatorController::attOnTxEnabled().
//   9. chkForceATTwhenPSAoff initialises from StepAttenuatorController::forceAttWhenPsOff().
//  10. chkForceATTwhenPSAoff toggle updates StepAttenuatorController::forceAttWhenPsOff().
//
// Thetis references:
//   Max Power slider — console.cs:4822 [v2.10.3.13] PWR setter
//   Tune power per band — console.cs:12094 [v2.10.3.13] tunePower_by_band[]
//   chkATTOnTX — setup.designer.cs:5926-5939 [v2.10.3.13] + setup.cs:15452-15455
//   chkForceATTwhenPSAoff — setup.designer.cs:5660-5671 [v2.10.3.13] + setup.cs:24264-24268
//   //MW0LGE [2.9.0.7] added  [original inline comment from console.cs:29285]

#include <QtTest/QtTest>
#include <QApplication>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSignalSpy>

#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "models/Band.h"
#include "core/HpsdrModel.h"
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

    void tunePwrSpin_initFromModel();
    void tunePwrSpin_changesModel();
    void tunePwrSpin_updatesFromModel();

    // Issue #175 Task 9 — per-band Tune Power grid SKU-aware (HL2 dB / others W).
    void perBandSpinboxes_hl2_dB();
    void perBandSpinboxes_anan100_W();

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
// 4. Per-band tune-power spinboxes initialise from TransmitModel
//    Cast: Issue #175 Task 9 flipped m_tunePwrSpins[] from QSpinBox* to
//    QDoubleSpinBox* so the per-SKU display semantics (dB on HL2, W
//    elsewhere) can vary the decimal count.  On non-HL2 SKUs the displayed
//    value still equals the stored watts, so the integer 17 round-trips.
// ---------------------------------------------------------------------------
void TestPowerPageH4::tunePwrSpin_initFromModel()
{
    RadioModel model;
    model.transmitModel().setTunePowerForBand(Band::Band20m, 17);

    PowerPage page(&model);
    page.show();

    const QString name = QStringLiteral("spinTunePwr_") + bandLabel(Band::Band20m);
    auto* spin = page.findChild<QDoubleSpinBox*>(name);
    QVERIFY(spin);
    QCOMPARE(spin->value(), 17.0);
}

// ---------------------------------------------------------------------------
// 5. Editing a spinbox updates TransmitModel::tunePowerForBand()
// ---------------------------------------------------------------------------
void TestPowerPageH4::tunePwrSpin_changesModel()
{
    RadioModel model;
    PowerPage page(&model);
    page.show();

    const QString name = QStringLiteral("spinTunePwr_") + bandLabel(Band::Band40m);
    auto* spin = page.findChild<QDoubleSpinBox*>(name);
    QVERIFY(spin);

    spin->setValue(25.0);
    QCOMPARE(model.transmitModel().tunePowerForBand(Band::Band40m), 25);
}

// ---------------------------------------------------------------------------
// 6. TransmitModel::tunePowerByBandChanged updates the spinbox (reverse)
// ---------------------------------------------------------------------------
void TestPowerPageH4::tunePwrSpin_updatesFromModel()
{
    RadioModel model;
    PowerPage page(&model);
    page.show();

    const QString name = QStringLiteral("spinTunePwr_") + bandLabel(Band::Band80m);
    auto* spin = page.findChild<QDoubleSpinBox*>(name);
    QVERIFY(spin);

    // Setting via TxApplet path (model setter) should propagate to setup page.
    model.transmitModel().setTunePowerForBand(Band::Band80m, 33);
    QCoreApplication::processEvents();
    QCOMPARE(spin->value(), 33.0);
}

// ---------------------------------------------------------------------------
// 4b. Issue #175 Task 9 — per-band grid SKU-aware: HL2 flips to dB display
// ---------------------------------------------------------------------------
void TestPowerPageH4::perBandSpinboxes_hl2_dB()
{
    RadioModel model;
    PowerPage page(&model);
    page.applyHpsdrModel(HPSDRModel::HERMESLITE);

    // Pick a band that's exercised by the per-band grid (HF amateur slot).
    auto* sb = page.findChild<QDoubleSpinBox*>(
        QStringLiteral("spinTunePwr_") + bandLabel(Band::Band40m));
    QVERIFY(sb != nullptr);
    QCOMPARE(sb->minimum(),    -16.5);
    QCOMPARE(sb->maximum(),      0.0);
    QCOMPARE(sb->singleStep(),   0.5);
    QCOMPARE(sb->decimals(),       1);
    QCOMPARE(sb->suffix(), QStringLiteral(" dB"));
}

// ---------------------------------------------------------------------------
// 4c. Issue #175 Task 9 — per-band grid SKU-aware: ANAN-100 stays in W
// ---------------------------------------------------------------------------
void TestPowerPageH4::perBandSpinboxes_anan100_W()
{
    RadioModel model;
    PowerPage page(&model);
    page.applyHpsdrModel(HPSDRModel::ANAN100);

    auto* sb = page.findChild<QDoubleSpinBox*>(
        QStringLiteral("spinTunePwr_") + bandLabel(Band::Band40m));
    QVERIFY(sb != nullptr);
    QCOMPARE(sb->minimum(),    0.0);
    QCOMPARE(sb->maximum(),  100.0);
    QCOMPARE(sb->decimals(),     0);
    QCOMPARE(sb->suffix(), QStringLiteral(" W"));
}

// ---------------------------------------------------------------------------
// 7. chkATTOnTX initialises from StepAttenuatorController::attOnTxEnabled()
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
