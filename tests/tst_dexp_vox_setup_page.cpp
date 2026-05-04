// tests/tst_dexp_vox_setup_page.cpp  (NereusSDR)
//
// Phase 3M-3a-iii Task 14 — DexpVoxPage full implementation.
//
// no-port-check: NereusSDR-original test file.  DexpVoxPage mirrors
// Thetis tpDSPVOXDE layout 1:1 (setup.designer.cs:44763-45260
// [v2.10.3.13]).  All control names match the Thetis Designer
// (chkVOXEnable, chkDEXPEnable, udDEXPThreshold, udDEXPHysteresisRatio,
// udDEXPExpansionRatio, udDEXPAttack, udDEXPHold, udDEXPRelease,
// udDEXPDetTau, chkDEXPLookAheadEnable, udDEXPLookAhead, chkSCFEnable,
// udSCFLowCut, udSCFHighCut).
//
// Coverage:
//   1. All 14 controls construct (VOX/DEXP 9 + Audio LookAhead 2 + SCF 3).
//   2. Each control's value reflects the initial TM default.
//   3. Setting each control updates the corresponding TM property.
//   4. Setting each TM property updates the control (other direction).
//   5. Group titles match Thetis verbatim.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QSignalSpy>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestDexpVoxSetupPage : public QObject
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

    // ── 1. All 14 controls construct ─────────────────────────────────────────
    void allControls_construct()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        // VOX / DEXP — 9 controls.
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkVOXEnable")));
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkDEXPEnable")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udDEXPThreshold")));
        QVERIFY(page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPHysteresisRatio")));
        QVERIFY(page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPExpansionRatio")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udDEXPAttack")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udDEXPHold")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udDEXPRelease")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udDEXPDetTau")));

        // Audio LookAhead — 2 controls.
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkDEXPLookAheadEnable")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udDEXPLookAhead")));

        // Side-Channel Trigger Filter — 3 controls.
        QVERIFY(page.findChild<QCheckBox*>(QStringLiteral("chkSCFEnable")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udSCFLowCut")));
        QVERIFY(page.findChild<QSpinBox*>(QStringLiteral("udSCFHighCut")));
    }

    // ── 2. Initial values mirror TM defaults ─────────────────────────────────
    //
    // Defaults all sourced from Thetis setup.designer.cs [v2.10.3.13]:
    //   voxEnabled               = false  (audio.cs:167 — vox always loads OFF)
    //   dexpEnabled              = false
    //   voxThresholdDb           = -40    (NereusSDR default; Thetis -20 — see TM.h:1576)
    //   dexpHysteresisRatioDb    = 2.0    (line 44869-44873)
    //   dexpExpansionRatioDb     = 10.0   (line 44900-44904)
    //   dexpAttackTimeMs         = 2.0    (line 45050-45054)
    //   voxHangTimeMs            = 500    (line 45020-45024 — shared with udDEXPHold)
    //   dexpReleaseTimeMs        = 100.0  (line 44990-44994)
    //   dexpDetectorTauMs        = 20.0   (line 45093-45097)
    //   dexpLookAheadEnabled     = true   (line 44808-44809)
    //   dexpLookAheadMs          = 60.0   (line 44788-44792)
    //   dexpSideChannelFilterEnabled = true   (line 45250-45251)
    //   dexpLowCutHz             = 500.0  (line 45240-45244)
    //   dexpHighCutHz            = 1500.0 (line 45210-45214)
    void initialValues_reflectTransmitModelDefaults()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkVOXEnable"))->isChecked(), false);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkDEXPEnable"))->isChecked(), false);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPThreshold"))->value(), -40);
        QCOMPARE(page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPHysteresisRatio"))->value(), 2.0);
        QCOMPARE(page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPExpansionRatio"))->value(), 10.0);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPAttack"))->value(), 2);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPHold"))->value(), 500);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPRelease"))->value(), 100);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPDetTau"))->value(), 20);

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkDEXPLookAheadEnable"))->isChecked(), true);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPLookAhead"))->value(), 60);

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkSCFEnable"))->isChecked(), true);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udSCFLowCut"))->value(), 500);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udSCFHighCut"))->value(), 1500);
    }

    // ── 3. Control → TransmitModel direction ─────────────────────────────────
    void controlToggle_updatesTransmitModel()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        TransmitModel& tx = model.transmitModel();

        // VOX enable
        page.findChild<QCheckBox*>(QStringLiteral("chkVOXEnable"))->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(tx.voxEnabled(), true);

        // DEXP enable
        page.findChild<QCheckBox*>(QStringLiteral("chkDEXPEnable"))->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpEnabled(), true);

        // Threshold (shared VOX/DEXP threshold dBV)
        page.findChild<QSpinBox*>(QStringLiteral("udDEXPThreshold"))->setValue(-25);
        QCoreApplication::processEvents();
        QCOMPARE(tx.voxThresholdDb(), -25);

        // Hysteresis Ratio
        page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPHysteresisRatio"))->setValue(3.5);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpHysteresisRatioDb(), 3.5);

        // Expansion Ratio
        page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPExpansionRatio"))->setValue(15.0);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpExpansionRatioDb(), 15.0);

        // Attack
        page.findChild<QSpinBox*>(QStringLiteral("udDEXPAttack"))->setValue(10);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpAttackTimeMs(), 10.0);

        // Hold (shared with VOX hang)
        page.findChild<QSpinBox*>(QStringLiteral("udDEXPHold"))->setValue(800);
        QCoreApplication::processEvents();
        QCOMPARE(tx.voxHangTimeMs(), 800);

        // Release
        page.findChild<QSpinBox*>(QStringLiteral("udDEXPRelease"))->setValue(250);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpReleaseTimeMs(), 250.0);

        // DetTau
        page.findChild<QSpinBox*>(QStringLiteral("udDEXPDetTau"))->setValue(40);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpDetectorTauMs(), 40.0);

        // LookAhead enable (toggle off then on so we see a change)
        page.findChild<QCheckBox*>(QStringLiteral("chkDEXPLookAheadEnable"))->setChecked(false);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpLookAheadEnabled(), false);

        // LookAhead ms
        page.findChild<QSpinBox*>(QStringLiteral("udDEXPLookAhead"))->setValue(120);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpLookAheadMs(), 120.0);

        // SCF enable (toggle off)
        page.findChild<QCheckBox*>(QStringLiteral("chkSCFEnable"))->setChecked(false);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpSideChannelFilterEnabled(), false);

        // SCF LowCut
        page.findChild<QSpinBox*>(QStringLiteral("udSCFLowCut"))->setValue(800);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpLowCutHz(), 800.0);

        // SCF HighCut
        page.findChild<QSpinBox*>(QStringLiteral("udSCFHighCut"))->setValue(2200);
        QCoreApplication::processEvents();
        QCOMPARE(tx.dexpHighCutHz(), 2200.0);
    }

    // ── 4. TransmitModel → control direction ─────────────────────────────────
    void modelChange_updatesControl()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        TransmitModel& tx = model.transmitModel();

        tx.setVoxEnabled(true);
        tx.setDexpEnabled(true);
        tx.setVoxThresholdDb(-15);
        tx.setDexpHysteresisRatioDb(4.2);
        tx.setDexpExpansionRatioDb(20.0);
        tx.setDexpAttackTimeMs(8.0);
        tx.setVoxHangTimeMs(900);
        tx.setDexpReleaseTimeMs(300.0);
        tx.setDexpDetectorTauMs(50.0);
        tx.setDexpLookAheadEnabled(false);
        tx.setDexpLookAheadMs(150.0);
        tx.setDexpSideChannelFilterEnabled(false);
        tx.setDexpLowCutHz(700.0);
        tx.setDexpHighCutHz(2500.0);
        QCoreApplication::processEvents();

        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkVOXEnable"))->isChecked(), true);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkDEXPEnable"))->isChecked(), true);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPThreshold"))->value(), -15);
        QCOMPARE(page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPHysteresisRatio"))->value(), 4.2);
        QCOMPARE(page.findChild<QDoubleSpinBox*>(QStringLiteral("udDEXPExpansionRatio"))->value(), 20.0);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPAttack"))->value(), 8);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPHold"))->value(), 900);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPRelease"))->value(), 300);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPDetTau"))->value(), 50);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkDEXPLookAheadEnable"))->isChecked(), false);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udDEXPLookAhead"))->value(), 150);
        QCOMPARE(page.findChild<QCheckBox*>(QStringLiteral("chkSCFEnable"))->isChecked(), false);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udSCFLowCut"))->value(), 700);
        QCOMPARE(page.findChild<QSpinBox*>(QStringLiteral("udSCFHighCut"))->value(), 2500);
    }

    // ── 5. Group titles match Thetis verbatim ────────────────────────────────
    //
    // From setup.designer.cs [v2.10.3.13]:
    //   grpDEXPVOX.Text       = "VOX / DEXP"               (line 44843)
    //   grpDEXPLookAhead.Text = "Audio LookAhead"          (line 44763)
    //   grpSCF.Text           = "Side-Channel Trigger Filter" (line 45165)
    void groupTitles_matchThetisVerbatim()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        const auto groups = page.findChildren<QGroupBox*>();
        QStringList titles;
        for (auto* g : groups) {
            titles << g->title();
        }
        QVERIFY2(titles.contains(QStringLiteral("VOX / DEXP")),
                 qPrintable("Missing 'VOX / DEXP' group; saw: " + titles.join(", ")));
        QVERIFY2(titles.contains(QStringLiteral("Audio LookAhead")),
                 qPrintable("Missing 'Audio LookAhead' group; saw: " + titles.join(", ")));
        QVERIFY2(titles.contains(QStringLiteral("Side-Channel Trigger Filter")),
                 qPrintable("Missing 'Side-Channel Trigger Filter' group; saw: " + titles.join(", ")));
    }
};

QTEST_MAIN(TestDexpVoxSetupPage)
#include "tst_dexp_vox_setup_page.moc"
