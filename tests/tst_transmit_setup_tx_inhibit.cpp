// tests/tst_transmit_setup_tx_inhibit.cpp  (NereusSDR)
//
// Phase 3M-0 Task 10 — External TX Inhibit group box on Setup → Transmit.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies grpExtTXInhibit contains the 2 controls per
// Thetis setup.designer.cs:46626-46657 [v2.10.3.13], and that toggling
// chkTXInhibit persists to AppSettings.

#include <QtTest>
#include <QGroupBox>
#include <QCheckBox>
#include "gui/setup/TransmitSetupPages.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestTransmitSetupTxInhibit : public QObject
{
    Q_OBJECT
private slots:
    void groupBoxBuilt_bothControlsPresent();
    void chkTXInhibit_toggle_persistsToAppSettings();
};

void TestTransmitSetupTxInhibit::groupBoxBuilt_bothControlsPresent()
{
    PowerPage page(/*model=*/nullptr);
    auto* group = page.findChild<QGroupBox*>("grpExtTXInhibit");
    QVERIFY(group);

    auto* chkInhibit = group->findChild<QCheckBox*>("chkTXInhibit");
    QVERIFY(chkInhibit);
    QCOMPARE(chkInhibit->isChecked(), false);
    QCOMPARE(chkInhibit->toolTip(), QString("Thetis will update on TX inhibit state change"));

    auto* chkReverse = group->findChild<QCheckBox*>("chkTXInhibitReverse");
    QVERIFY(chkReverse);
    QCOMPARE(chkReverse->isChecked(), false);
    QCOMPARE(chkReverse->toolTip(), QString("Reverse the input state logic"));
}

void TestTransmitSetupTxInhibit::chkTXInhibit_toggle_persistsToAppSettings()
{
    // Clear any leftover key from a previous run in the sandbox
    AppSettings::instance().setValue(QStringLiteral("TxInhibitMonitorEnabled"), QStringLiteral("False"));

    PowerPage page(/*model=*/nullptr);
    auto* group = page.findChild<QGroupBox*>("grpExtTXInhibit");
    QVERIFY(group);
    auto* chk = group->findChild<QCheckBox*>("chkTXInhibit");
    QVERIFY(chk);

    // Toggle on
    chk->setChecked(true);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("TxInhibitMonitorEnabled")).toString(),
             QString("True"));

    // Toggle off
    chk->setChecked(false);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("TxInhibitMonitorEnabled")).toString(),
             QString("False"));
}

QTEST_MAIN(TestTransmitSetupTxInhibit)
#include "tst_transmit_setup_tx_inhibit.moc"
