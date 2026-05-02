// tests/tst_transmit_setup_block_tx.cpp  (NereusSDR)
//
// Phase 3M-0 Task 11 — Block-TX antennas + Disable HF PA group boxes on
// Setup → Transmit.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies:
//   chkBlockTxAnt2 — NereusSDR-original label and tooltip (Thetis ships
//     unlabelled column-header checkboxes per
//     setup.designer.cs:6715-6724 [v2.10.3.13])
//   chkBlockTxAnt3 — same rationale, per
//     setup.designer.cs:6704-6713 [v2.10.3.13]
//   chkHFTRRelay   — "Disables HF PA." verbatim from
//     setup.designer.cs:5789 [v2.10.3.13]

#include <QtTest>
#include <QGroupBox>
#include <QCheckBox>
#include "gui/setup/TransmitSetupPages.h"

using namespace NereusSDR;

class TestTransmitSetupBlockTx : public QObject
{
    Q_OBJECT
private slots:
    void blockTxAnt2_present_withNereusOriginalLabel();
    void blockTxAnt3_present_withNereusOriginalLabel();
    void chkHFTRRelay_present_withThetisTooltip();
};

void TestTransmitSetupBlockTx::blockTxAnt2_present_withNereusOriginalLabel()
{
    PowerPage page(/*model=*/nullptr);
    auto* group = page.findChild<QGroupBox*>("grpBlockTxAntennas");
    QVERIFY(group);

    auto* chk = group->findChild<QCheckBox*>("chkBlockTxAnt2");
    QVERIFY(chk);
    QCOMPARE(chk->isChecked(), false);
    // NereusSDR-original label — Thetis has no text on this checkbox
    QCOMPARE(chk->text(), QString("Block TX on Ant 2"));
    // NereusSDR-original tooltip — Thetis has no tooltip on this checkbox
    QCOMPARE(chk->toolTip(), QString("When checked, the radio cannot transmit on Antenna 2"));
}

void TestTransmitSetupBlockTx::blockTxAnt3_present_withNereusOriginalLabel()
{
    PowerPage page(/*model=*/nullptr);
    auto* group = page.findChild<QGroupBox*>("grpBlockTxAntennas");
    QVERIFY(group);

    auto* chk = group->findChild<QCheckBox*>("chkBlockTxAnt3");
    QVERIFY(chk);
    QCOMPARE(chk->isChecked(), false);
    // NereusSDR-original label — Thetis has no text on this checkbox
    QCOMPARE(chk->text(), QString("Block TX on Ant 3"));
    // NereusSDR-original tooltip — Thetis has no tooltip on this checkbox
    QCOMPARE(chk->toolTip(), QString("When checked, the radio cannot transmit on Antenna 3"));
}

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
