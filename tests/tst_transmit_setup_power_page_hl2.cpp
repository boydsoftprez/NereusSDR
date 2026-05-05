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

#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
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

QTEST_MAIN(TestTransmitSetupPowerPageHl2)
#include "tst_transmit_setup_power_page_hl2.moc"
