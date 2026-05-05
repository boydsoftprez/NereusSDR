// tests/tst_transmit_setup_block_tx.cpp  (NereusSDR)
//
// Phase 3M-0 Task 11 — Disable HF PA group box on Setup → Transmit.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies:
//   chkHFTRRelay   — "Disables HF PA." verbatim from
//     setup.designer.cs:5789 [v2.10.3.13]
//
// Issue #175 Wave 1: chkBlockTxAnt2 / chkBlockTxAnt3 cases removed —
// the dropped Power-page checkboxes were writing dead AppSettings keys
// (AlexAnt2RxOnly / AlexAnt3RxOnly) that nothing read.  Live editor at
// AntennaAlexAntennaControlTab uses AlexController's per-MAC keys; that
// page has its own coverage.

#include <QtTest>
#include <QGroupBox>
#include <QCheckBox>
#include "gui/setup/TransmitSetupPages.h"

using namespace NereusSDR;

class TestTransmitSetupBlockTx : public QObject
{
    Q_OBJECT
private slots:
    void chkHFTRRelay_present_withThetisTooltip();
};

void TestTransmitSetupBlockTx::chkHFTRRelay_present_withThetisTooltip()
{
    PowerPage page(/*model=*/nullptr);
    auto* group = page.findChild<QGroupBox*>("grpHfPaControl");
    QVERIFY(group);

    auto* chk = group->findChild<QCheckBox*>("chkHFTRRelay");
    QVERIFY(chk);
    QCOMPARE(chk->isChecked(), false);
    QCOMPARE(chk->text(), QString("Disable HF PA"));
    // Verbatim from Thetis setup.designer.cs:5789 [v2.10.3.13]
    QCOMPARE(chk->toolTip(), QString("Disables HF PA."));
}

QTEST_MAIN(TestTransmitSetupBlockTx)
#include "tst_transmit_setup_block_tx.moc"
