// tests/tst_cfc_setup_page.cpp  (NereusSDR)
//
// Phase 3M-3a-ii Batch 5 (Task E) — CfcSetupPage full implementation.
//
// no-port-check: NereusSDR-original test file.  CfcSetupPage mirrors
// Thetis tpDSPCFC layout 1:1 (setup.Designer.cs:46162-46280 [v2.10.3.13]
// for grpPhRot; remaining groups follow TransmitModel kCfc* schema added
// in Batch 2).  All control names match the Thetis Designer (chkPHROTEnable,
// chkCFCEnable, etc.).
//
// Coverage:
//   1. All 9 controls construct (PhRot 4 + CFC 4 incl. button + CESSB 1).
//   2. Each control's value reflects the initial TM default.
//   3. Setting each control updates the corresponding TM property.
//   4. Setting each TM property updates the control (other direction).
//   5. Range clamping: spinbox refuses out-of-range writes.
//   6. The [Configure CFC bands…] button emits openCfcDialogRequested
//      exactly once when clicked.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "gui/setup/DspSetupPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestCfcSetupPage : public QObject
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

    // ── 1. All 9 controls construct ──────────────────────────────────────────
    void allControls_construct()
    {
        RadioModel model;
        CfcSetupPage page(&model);
        page.show();

        // PhRot — 4 controls.
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkPHROTEnable")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udPhRotFreq")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udPHROTStages")));
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkPHROTReverse")));

        // CFC — 4 controls + dialog button.
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkCFCEnable")));
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkCFCPeqEnable")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udCFCPreComp")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udCFCPostEqGain")));
        QVERIFY(page.findChild<QPushButton*>(QStringLiteral("btnCFCBandsConfigure")));

        // CESSB — 1 control.
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkCESSBEnable")));
    }

    // ── 2. Initial values mirror TM defaults ─────────────────────────────────
    //
    // From TransmitModel.h:1297-1335 [Phase 3M-3a-ii Batch 2 schema]:
    //   phaseRotatorEnabled = false,  phaseReverseEnabled = false
    //   phaseRotatorFreqHz  = 338,    phaseRotatorStages  = 8
    //   cfcEnabled          = false,  cfcPostEqEnabled    = false
    //   cfcPrecompDb        = 0,      cfcPostEqGainDb     = 0
    //   cessbOn             = false
    void initialValues_reflectTransmitModelDefaults()
    {
        RadioModel model;
        CfcSetupPage page(&model);
        page.show();

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkPHROTEnable"))->isChecked(), false);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udPhRotFreq"))->value(), 338);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udPHROTStages"))->value(), 8);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkPHROTReverse"))->isChecked(), false);

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkCFCEnable"))->isChecked(), false);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkCFCPeqEnable"))->isChecked(), false);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udCFCPreComp"))->value(), 0);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udCFCPostEqGain"))->value(), 0);

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkCESSBEnable"))->isChecked(), false);
    }

    // ── 3. Control → TransmitModel direction ─────────────────────────────────
    void controlToggle_updatesTransmitModel()
    {
        RadioModel model;
        CfcSetupPage page(&model);
        page.show();

        TransmitModel& tx = model.transmitModel();

        // PhRot enable
        page.findChild<QCheckBox*>(QStringLiteral("chkPHROTEnable"))->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(tx.phaseRotatorEnabled(), true);

        // PhRot reverse
        page.findChild<QCheckBox*>(QStringLiteral("chkPHROTReverse"))->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(tx.phaseReverseEnabled(), true);

        // PhRot freq
        page.findChild<QSpinBox*>(QStringLiteral("udPhRotFreq"))->setValue(750);
        QCoreApplication::processEvents();
        QCOMPARE(tx.phaseRotatorFreqHz(), 750);

        // PhRot stages
        page.findChild<QSpinBox*>(QStringLiteral("udPHROTStages"))->setValue(12);
        QCoreApplication::processEvents();
        QCOMPARE(tx.phaseRotatorStages(), 12);

        // CFC enable
        page.findChild<QCheckBox*>(QStringLiteral("chkCFCEnable"))->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(tx.cfcEnabled(), true);

        // CFC Post-EQ enable
        page.findChild<QCheckBox*>(QStringLiteral("chkCFCPeqEnable"))->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(tx.cfcPostEqEnabled(), true);

        // CFC pre-comp
        page.findChild<QSpinBox*>(QStringLiteral("udCFCPreComp"))->setValue(8);
        QCoreApplication::processEvents();
        QCOMPARE(tx.cfcPrecompDb(), 8);

        // CFC post-EQ gain
        page.findChild<QSpinBox*>(QStringLiteral("udCFCPostEqGain"))->setValue(-6);
        QCoreApplication::processEvents();
        QCOMPARE(tx.cfcPostEqGainDb(), -6);

        // CESSB enable
        page.findChild<QCheckBox*>(QStringLiteral("chkCESSBEnable"))->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(tx.cessbOn(), true);
    }

    // ── 4. TransmitModel → control direction ─────────────────────────────────
    void modelChange_updatesControl()
    {
        RadioModel model;
        CfcSetupPage page(&model);
        page.show();

        TransmitModel& tx = model.transmitModel();

        // PhRot
        tx.setPhaseRotatorEnabled(true);
        tx.setPhaseReverseEnabled(true);
        tx.setPhaseRotatorFreqHz(450);
        tx.setPhaseRotatorStages(10);
        QCoreApplication::processEvents();

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkPHROTEnable"))->isChecked(), true);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkPHROTReverse"))->isChecked(), true);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udPhRotFreq"))->value(), 450);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udPHROTStages"))->value(), 10);

        // CFC
        tx.setCfcEnabled(true);
        tx.setCfcPostEqEnabled(true);
        tx.setCfcPrecompDb(12);
        tx.setCfcPostEqGainDb(15);
        QCoreApplication::processEvents();

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkCFCEnable"))->isChecked(), true);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkCFCPeqEnable"))->isChecked(), true);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udCFCPreComp"))->value(), 12);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udCFCPostEqGain"))->value(), 15);

        // CESSB
        tx.setCessbOn(true);
        QCoreApplication::processEvents();

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkCESSBEnable"))->isChecked(), true);
    }

    // ── 5. Range clamping — spinbox refuses out-of-range writes ──────────────
    //
    // Ranges from TransmitModel constants:
    //   kPhaseRotatorFreqHz        [10, 2000]
    //   kPhaseRotatorStages        [2, 16]
    //   kCfcPrecompDb              [0, 16]
    //   kCfcPostEqGainDb           [-24, 24]
    void spinboxes_clampToRange()
    {
        RadioModel model;
        CfcSetupPage page(&model);
        page.show();

        auto* freq    = page.findChild<QSpinBox*>(QStringLiteral("udPhRotFreq"));
        auto* stages  = page.findChild<QSpinBox*>(QStringLiteral("udPHROTStages"));
        auto* preComp = page.findChild<QSpinBox*>(QStringLiteral("udCFCPreComp"));
        auto* postEq  = page.findChild<QSpinBox*>(QStringLiteral("udCFCPostEqGain"));

        // Below floor — Qt clamps to the spinbox's min.
        freq->setValue(-100);
        QCOMPARE(freq->value(), TransmitModel::kPhaseRotatorFreqHzMin);    // 10
        // Above ceiling.
        freq->setValue(99999);
        QCOMPARE(freq->value(), TransmitModel::kPhaseRotatorFreqHzMax);    // 2000

        stages->setValue(0);
        QCOMPARE(stages->value(), TransmitModel::kPhaseRotatorStagesMin);  // 2
        stages->setValue(99);
        QCOMPARE(stages->value(), TransmitModel::kPhaseRotatorStagesMax);  // 16

        preComp->setValue(-50);
        QCOMPARE(preComp->value(), TransmitModel::kCfcPrecompDbMin);       // 0
        preComp->setValue(50);
        QCOMPARE(preComp->value(), TransmitModel::kCfcPrecompDbMax);       // 16

        postEq->setValue(-99);
        QCOMPARE(postEq->value(), TransmitModel::kCfcPostEqGainDbMin);     // -24
        postEq->setValue(99);
        QCOMPARE(postEq->value(), TransmitModel::kCfcPostEqGainDbMax);     // 24
    }

    // ── 6. [Configure CFC bands…] button emits openCfcDialogRequested ────────
    //
    // Batch 6 will wire the parent SetupDialog to route this signal to the
    // modeless TxCfcDialog.  For Batch 5 we just verify the signal contract.
    void configureCfcBandsButton_emitsSignal()
    {
        RadioModel model;
        CfcSetupPage page(&model);
        page.show();

        QSignalSpy spy(&page, &CfcSetupPage::openCfcDialogRequested);

        auto* btn = page.findChild<QPushButton*>(QStringLiteral("btnCFCBandsConfigure"));
        QVERIFY(btn);
        QVERIFY(btn->isEnabled());

        btn->click();
        QCOMPARE(spy.count(), 1);

        // Click again — single-shot per click.
        btn->click();
        QCOMPARE(spy.count(), 2);
    }
};

QTEST_MAIN(TestCfcSetupPage)
#include "tst_cfc_setup_page.moc"
