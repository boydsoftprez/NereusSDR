// tests/tst_general_setup_options.cpp  (NereusSDR)
//
// Phase 3M-0 Task 13 — Options group box on Setup → General.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies grpGeneralOptions contains chkPreventTXonDifferentBandToRX per
// Thetis setup.designer.cs:9050-9059 [v2.10.3.13].
// Note: tooltip is NereusSDR-original — Thetis has no tooltip on this control.

#include <QtTest>
#include <QGroupBox>
#include <QCheckBox>
#include "gui/setup/GeneralOptionsPage.h"

using namespace NereusSDR;

class TestGeneralSetupOptions : public QObject
{
    Q_OBJECT
private slots:
    void preventTxOnDifferentBand_present_unchecked();
};

void TestGeneralSetupOptions::preventTxOnDifferentBand_present_unchecked()
{
    // From Thetis setup.designer.cs:9050-9059 [v2.10.3.13]
    GeneralOptionsPage page(/*model=*/nullptr);
    auto* group = page.findChild<QGroupBox*>("grpGeneralOptions");
    QVERIFY2(group, "grpGeneralOptions not found");

    auto* chk = group->findChild<QCheckBox*>("chkPreventTXonDifferentBandToRX");
    QVERIFY2(chk, "chkPreventTXonDifferentBandToRX not found");
    QCOMPARE(chk->text(), QString("Prevent TX'ing on a different band to the RX band"));
    QCOMPARE(chk->isChecked(), false);
    // Tooltip is NereusSDR-original — Thetis has no tooltip on this control.
    QVERIFY2(!chk->toolTip().isEmpty(), "chkPreventTXonDifferentBandToRX must have a tooltip");
}

QTEST_MAIN(TestGeneralSetupOptions)
#include "tst_general_setup_options.moc"
