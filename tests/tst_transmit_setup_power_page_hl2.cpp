// no-port-check: unit tests for PowerPage grpPATune Tune group port (Task 8).
//
// Phase 3M-0 Issue #175 Task 8 — verifies the new "Tune" group box
// (mi0bot grpPATune) on Setup → Transmit → Power:
//   - QGroupBox objectName="grpPATune", title="Tune"
//   - 3 drive-source radios: radUseFixedDriveTune, radUseDriveSliderTune,
//     radUseTuneSliderTune
//   - QComboBox objectName="comboTXTUNMeter"
//   - QDoubleSpinBox objectName="udTXTunePower" with SKU-polymorphed bounds
//
// Source cites:
//   mi0bot-Thetis setup.designer.cs:47874-47930 [v2.10.3.13-beta2] (group layout)
//   mi0bot-Thetis setup.cs:20328-20331         [v2.10.3.13-beta2] (HL2 spinbox)

#include <QtTest/QtTest>
#include <QApplication>
#include <QGroupBox>
#include <QRadioButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>

#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "models/Band.h"
#include "core/HpsdrModel.h"

using NereusSDR::HPSDRModel;

class TestTransmitSetupPowerPageHl2 : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();

    void tuneGroup_exists();
    void tuneGroup_hasThreeRadios();
    void tuneGroup_hasMeterCombo();
    void tuneGroup_fixedSpinbox_hl2();
    void tuneGroup_fixedSpinbox_anan100();
    void tuneGroup_radioToggle_persists();
    void tuneGroup_fixedSpinbox_disabledWhenNotFixed();

    // Issue #175 follow-up — Reset Tune Power Defaults button.
    void resetButton_exists();
    void resetButton_restoresDefaults_hl2();
    void resetButton_restoresDefaults_anan100();
    void resetButton_restoresDriveSourceAndMeter();
};

void TestTransmitSetupPowerPageHl2::initTestCase()
{
    if (!qApp) {
        static int   argc = 0;
        static char* argv = nullptr;
        new QApplication(argc, &argv);
    }
}

void TestTransmitSetupPowerPageHl2::tuneGroup_exists()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    QGroupBox* g = page.findChild<QGroupBox*>(QStringLiteral("grpPATune"));
    QVERIFY(g != nullptr);
    QCOMPARE(g->title(), QStringLiteral("Tune"));
}

void TestTransmitSetupPowerPageHl2::tuneGroup_hasThreeRadios()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    QVERIFY(page.findChild<QRadioButton*>(QStringLiteral("radUseFixedDriveTune")));
    QVERIFY(page.findChild<QRadioButton*>(QStringLiteral("radUseDriveSliderTune")));
    QVERIFY(page.findChild<QRadioButton*>(QStringLiteral("radUseTuneSliderTune")));
}

void TestTransmitSetupPowerPageHl2::tuneGroup_hasMeterCombo()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    QVERIFY(page.findChild<QComboBox*>(QStringLiteral("comboTXTUNMeter")));
}

void TestTransmitSetupPowerPageHl2::tuneGroup_fixedSpinbox_hl2()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    page.applyHpsdrModel(HPSDRModel::HERMESLITE);
    auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
    QVERIFY(sb != nullptr);
    QCOMPARE(sb->minimum(), -16.5);
    QCOMPARE(sb->maximum(),   0.0);
    QCOMPARE(sb->singleStep(), 0.5);
    QCOMPARE(sb->decimals(),    1);
    QCOMPARE(sb->suffix(), QStringLiteral(" dB"));
}

void TestTransmitSetupPowerPageHl2::tuneGroup_fixedSpinbox_anan100()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    page.applyHpsdrModel(HPSDRModel::ANAN100);
    auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
    QVERIFY(sb != nullptr);
    QCOMPARE(sb->minimum(),    0.0);
    QCOMPARE(sb->maximum(),  100.0);
    QCOMPARE(sb->singleStep(), 1.0);
    QCOMPARE(sb->decimals(),     0);
    QCOMPARE(sb->suffix(), QStringLiteral(" W"));
}

void TestTransmitSetupPowerPageHl2::tuneGroup_radioToggle_persists()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    auto* radFixed = page.findChild<QRadioButton*>(QStringLiteral("radUseFixedDriveTune"));
    QVERIFY(radFixed != nullptr);
    radFixed->setChecked(true);
    QCOMPARE(rm.transmitModel().tuneDrivePowerSource(),
             NereusSDR::DrivePowerSource::Fixed);
}

void TestTransmitSetupPowerPageHl2::tuneGroup_fixedSpinbox_disabledWhenNotFixed()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    auto* radTune  = page.findChild<QRadioButton*>(QStringLiteral("radUseTuneSliderTune"));
    QVERIFY(radTune != nullptr);
    radTune->setChecked(true);
    auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
    QVERIFY(sb != nullptr);
    QVERIFY(!sb->isEnabled());
}

// ─────────────────────────────────────────────────────────────────────────
// Issue #175 follow-up — Reset Tune Power Defaults button
// ─────────────────────────────────────────────────────────────────────────

void TestTransmitSetupPowerPageHl2::resetButton_exists()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);
    auto* btn = page.findChild<QPushButton*>(QStringLiteral("btnResetTunePowerDefaults"));
    QVERIFY(btn != nullptr);
    QCOMPARE(btn->text(), QStringLiteral("Reset Tune Power Defaults"));
}

void TestTransmitSetupPowerPageHl2::resetButton_restoresDefaults_hl2()
{
    NereusSDR::RadioModel rm;
    rm.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
    NereusSDR::PowerPage page(&rm);

    NereusSDR::TransmitModel& tx = rm.transmitModel();
    tx.setHpsdrModel(HPSDRModel::HERMESLITE);

    // Pick a non-default Fixed Tune Power and a non-default per-band power.
    // HL2 storage is int 0..99 — anything other than 54 / 81 is fine.
    tx.setTunePower(20);                              // ≠ HL2 default 54
    tx.setTunePowerForBand(NereusSDR::Band::Band20m, 10);  // ≠ HL2 default 81

    // Click the reset button.
    auto* btn = page.findChild<QPushButton*>(QStringLiteral("btnResetTunePowerDefaults"));
    QVERIFY(btn != nullptr);
    btn->click();

    // Fixed Tune Power and per-band Tune Power both back to HL2 defaults.
    QCOMPARE(tx.tunePower(), 54);                                  // -7.5 dB
    QCOMPARE(tx.tunePowerForBand(NereusSDR::Band::Band20m), 81);   // -3.0 dB

    // The Fixed spinbox display reflects the new stored value via the
    // mi0bot dB conversion: (54/3 - 33)/2 = -7.5.
    auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
    QVERIFY(sb != nullptr);
    QCOMPARE(sb->value(), -7.5);
}

void TestTransmitSetupPowerPageHl2::resetButton_restoresDefaults_anan100()
{
    NereusSDR::RadioModel rm;
    rm.setHpsdrModelForTest(HPSDRModel::ANAN100);
    NereusSDR::PowerPage page(&rm);

    NereusSDR::TransmitModel& tx = rm.transmitModel();
    tx.setHpsdrModel(HPSDRModel::ANAN100);

    // Pick non-default values (defaults are 10 W / 50 W on ANAN100).
    tx.setTunePower(75);
    tx.setTunePowerForBand(NereusSDR::Band::Band40m, 25);

    auto* btn = page.findChild<QPushButton*>(QStringLiteral("btnResetTunePowerDefaults"));
    QVERIFY(btn != nullptr);
    btn->click();

    QCOMPARE(tx.tunePower(), 10);
    QCOMPARE(tx.tunePowerForBand(NereusSDR::Band::Band40m), 50);

    auto* sb = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
    QVERIFY(sb != nullptr);
    QCOMPARE(sb->value(), 10.0);  // displayed = stored on non-HL2
}

void TestTransmitSetupPowerPageHl2::resetButton_restoresDriveSourceAndMeter()
{
    NereusSDR::RadioModel rm;
    NereusSDR::PowerPage page(&rm);

    // Switch drive source to Fixed and TX TUN Meter combo away from index 0.
    auto* radFixed = page.findChild<QRadioButton*>(QStringLiteral("radUseFixedDriveTune"));
    QVERIFY(radFixed != nullptr);
    radFixed->setChecked(true);
    QCOMPARE(rm.transmitModel().tuneDrivePowerSource(),
             NereusSDR::DrivePowerSource::Fixed);

    auto* combo = page.findChild<QComboBox*>(QStringLiteral("comboTXTUNMeter"));
    QVERIFY(combo != nullptr);
    combo->setCurrentIndex(2);  // SWR
    QCOMPARE(combo->currentIndex(), 2);

    // Click Reset.
    auto* btn = page.findChild<QPushButton*>(QStringLiteral("btnResetTunePowerDefaults"));
    QVERIFY(btn != nullptr);
    btn->click();

    // Drive source restored to Tune Slider; meter combo restored to index 0.
    QCOMPARE(rm.transmitModel().tuneDrivePowerSource(),
             NereusSDR::DrivePowerSource::TuneSlider);
    auto* radTune = page.findChild<QRadioButton*>(QStringLiteral("radUseTuneSliderTune"));
    QVERIFY(radTune != nullptr);
    QVERIFY(radTune->isChecked());
    QCOMPARE(combo->currentIndex(), 0);
}

QTEST_MAIN(TestTransmitSetupPowerPageHl2)
#include "tst_transmit_setup_power_page_hl2.moc"
