// tests/tst_transmit_setup_swr_protection.cpp  (NereusSDR)
//
// Phase 3M-0 Task 9 — SWR Protection group box on Setup → Transmit.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies grpSWRProtectionControl contains the 5 controls per
// Thetis setup.designer.cs:5793-5924 [v2.10.3.13].

#include <QtTest>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include "gui/setup/TransmitSetupPages.h"

using namespace NereusSDR;

class TestTransmitSetupSwrProtection : public QObject
{
    Q_OBJECT
private slots:
    void groupBoxBuilt_fiveControlsPresent();
};

void TestTransmitSetupSwrProtection::groupBoxBuilt_fiveControlsPresent()
{
    PowerPage page(/*model=*/nullptr);
    auto* group = page.findChild<QGroupBox*>("grpSWRProtectionControl");
    QVERIFY(group);

    auto* chkProt = group->findChild<QCheckBox*>("chkSWRProtection");
    QVERIFY(chkProt);
    QCOMPARE(chkProt->isChecked(), false);
    QCOMPARE(chkProt->toolTip(), QString("Show a visual SWR warning in the spectral area"));

    auto* udLimit = group->findChild<QDoubleSpinBox*>("udSwrProtectionLimit");
    QVERIFY(udLimit);
    QCOMPARE(udLimit->value(), 2.0);
    QCOMPARE(udLimit->minimum(), 1.0);
    QCOMPARE(udLimit->maximum(), 5.0);

    auto* chkTune = group->findChild<QCheckBox*>("chkSWRTuneProtection");
    QVERIFY(chkTune);
    QCOMPARE(chkTune->toolTip(), QString("Disables SWR Protection during Tune."));

    auto* udIgnore = group->findChild<QSpinBox*>("udTunePowerSwrIgnore");
    QVERIFY(udIgnore);
    QCOMPARE(udIgnore->value(), 35);

    auto* chkWind = group->findChild<QCheckBox*>("chkWindBackPowerSWR");
    QVERIFY(chkWind);
    QCOMPARE(chkWind->toolTip(), QString("Winds back the power if high swr protection kicks in"));
}

QTEST_MAIN(TestTransmitSetupSwrProtection)
#include "tst_transmit_setup_swr_protection.moc"
