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
#include <QLabel>
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

    // ── 6-9. Phase 3M-3a-iv Task 10: Anti-VOX Tau (ms) spinbox ───────────────
    //
    // The Anti-VOX group lives below the 2x2 DEXP/VOX grid.  It currently
    // hosts a single Tau (ms) spinbox; remaining Thetis grpAntiVOX controls
    // (Enable / Source / Gain) ship in a follow-up sub-task.

    // Default value: 20 ms — Thetis udAntiVoxTau.Value (setup.designer.cs:44682
    // [v2.10.3.13]) and TransmitModel::kAntiVoxTauMsDefault.
    void tauSpinbox_defaultIs20()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxTau"));
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), 20);
    }

    // Spinbox -> TransmitModel direction.
    void tauSpinbox_writesTransmitModel()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxTau"));
        QVERIFY(spin != nullptr);

        spin->setValue(80);
        QCoreApplication::processEvents();

        QCOMPARE(model.transmitModel().antiVoxTauMs(), 80);
    }

    // TransmitModel -> spinbox direction.  QSignalBlocker round-trip should
    // not echo back into TM.
    void tauSpinbox_readsTransmitModel()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxTau"));
        QVERIFY(spin != nullptr);

        model.transmitModel().setAntiVoxTauMs(120);
        QCoreApplication::processEvents();

        QCOMPARE(spin->value(), 120);
        QCOMPARE(model.transmitModel().antiVoxTauMs(), 120);
    }

    // Tooltip is the Thetis verbatim string (no source-citation embedded in
    // the user-visible text — that goes in the source comment next to the
    // setToolTip call).
    void tauSpinbox_tooltipMatchesThetisVerbatim()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxTau"));
        QVERIFY(spin != nullptr);
        // From Thetis setup.designer.cs:44681 [v2.10.3.13].
        QCOMPARE(spin->toolTip(),
                 QStringLiteral("Time-constant used in smoothing Anti-VOX data"));
    }

    // ── Phase 3M-3a-iv scope-expansion: chkAntiVoxEnable / udAntiVoxGain
    //    land in grpAntiVOX above the existing Tau row ────────────────────
    //
    // Layout order (matching Thetis Y-coord ordering at
    // setup.designer.cs:44650 / 44666 / 44707 / 44744 [v2.10.3.13]):
    //   Y=19  chkAntiVoxEnable     ("Anti-VOX Enable")
    //   Y=41  [Source info row]    NereusSDR-spin replacing chkAntiVoxSource
    //   Y=71  udAntiVoxGain        ("Gain (dB)")
    //   Y=96  udAntiVoxTau         ("Tau (ms)")  -- existing
    //
    // 3M-3a-iv post-bench refactor (Option A): the Y=41 chkAntiVoxSource
    // checkbox has been replaced with a static info-row label.  Thetis
    // chkAntiVoxSource (RX vs VAC at setup.designer.cs:44646-44657
    // [v2.10.3.13]) does not map to NereusSDR's architecture (VAX is a
    // digital-mode app bus with no mic-feedback path).
    //
    // Defaults verified against Thetis chkAntiVoxEnable (initially unchecked
    // -- no Checked= setter), udAntiVoxGain (Value=10 -- but NereusSDR-original
    // divergence retains shipped default 0; see TransmitModel C.4 comment).

    void enableCheckbox_defaultUnchecked()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkAntiVoxEnable"));
        QVERIFY(chk != nullptr);
        QCOMPARE(chk->isChecked(), false);
        QCOMPARE(chk->text(), QStringLiteral("Anti-VOX Enable"));
    }

    void enableCheckbox_writesTransmitModel()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkAntiVoxEnable"));
        QVERIFY(chk != nullptr);

        chk->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(model.transmitModel().antiVoxRun(), true);
    }

    void enableCheckbox_readsTransmitModel()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkAntiVoxEnable"));
        QVERIFY(chk != nullptr);

        model.transmitModel().setAntiVoxRun(true);
        QCoreApplication::processEvents();

        QCOMPARE(chk->isChecked(), true);
    }

    void enableCheckbox_tooltipMatchesThetisVerbatim()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* chk = page.findChild<QCheckBox*>(QStringLiteral("chkAntiVoxEnable"));
        QVERIFY(chk != nullptr);
        // From Thetis setup.designer.cs:44749 [v2.10.3.13].
        QCOMPARE(chk->toolTip(),
                 QStringLiteral("Enable prevention measures for RX audio tripping VOX"));
    }

    // 3M-3a-iv post-bench refactor (Option A): sourceCheckbox_defaultUnchecked
    // and sourceCheckbox_tooltipStartsWithVaxLabel removed alongside the
    // chkAntiVoxSource checkbox.  Replaced with a static info-row label
    // (lblAntiVoxSourceInfo) verified by sourceInfoRow_* below.

    void sourceInfoRow_existsAndShowsExpectedText()
    {
        // NereusSDR-original info row replaces Thetis chkAntiVoxSource
        // (setup.designer.cs:44646-44657 [v2.10.3.13]) per the 3M-3a-iv
        // post-bench Option A refactor.  Anti-VOX always references the
        // audio output device(s) — VAX is a digital-mode app bus with no
        // mic-feedback path, so there is no user choice to expose.
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblAntiVoxSourceInfo"));
        QVERIFY(lbl != nullptr);
        QCOMPARE(lbl->text(), QStringLiteral("Source: Audio Output Device(s)"));
    }

    void sourceInfoRow_isStaticLabelNotCheckbox()
    {
        // The Y=41 grpAntiVOX row must be a read-only QLabel, NOT a
        // QCheckBox.  This guards against regression: anyone re-adding a
        // chkAntiVoxSource toggle would re-introduce the "VAX as anti-VOX
        // source" misconception we deliberately closed in the Option A
        // refactor.  See class header + design doc §18 [v2.10.3.13].
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        // The expected info-row widget is a QLabel.
        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblAntiVoxSourceInfo"));
        QVERIFY(lbl != nullptr);

        // No QCheckBox with the historical objectName must exist on the
        // page — its presence would mean the checkbox came back.
        auto* oldCheckbox = page.findChild<QCheckBox*>(QStringLiteral("chkAntiVoxSource"));
        QVERIFY(oldCheckbox == nullptr);
    }

    void sourceInfoRow_tooltipExplainsArchitecturalDivergence()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* lbl = page.findChild<QLabel*>(QStringLiteral("lblAntiVoxSourceInfo"));
        QVERIFY(lbl != nullptr);
        // The tooltip should explain why this is an info row rather than a
        // Thetis-style RX/VAC selector, in the operator's words: mention VAX
        // explicitly and say the output device is the only reference, so the
        // operator understands why no toggle is offered. The Thetis control
        // it replaces (chkAntiVoxSource) is named in the source comment
        // beside the tooltip, never in it (plain operator words, 2026-09-28).
        const QString tip = lbl->toolTip();
        QVERIFY(tip.contains(QStringLiteral("VAX")));
        QVERIFY(tip.contains(QStringLiteral("speaker bleed")));
        QVERIFY(tip.contains(QStringLiteral("only valid cancellation reference")));
        QVERIFY(!tip.contains(QStringLiteral("chkAntiVoxSource")));
    }

    void gainSpinbox_defaultZero()
    {
        // NereusSDR-original divergence: int default 0 (vs Thetis decimal 10).
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxGain"));
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->value(), 0);
        QCOMPARE(spin->minimum(), -60);
        QCOMPARE(spin->maximum(), 60);
    }

    void gainSpinbox_writesTransmitModel()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxGain"));
        QVERIFY(spin != nullptr);

        spin->setValue(-12);
        QCoreApplication::processEvents();

        QCOMPARE(model.transmitModel().antiVoxGainDb(), -12);
    }

    void gainSpinbox_readsTransmitModel()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxGain"));
        QVERIFY(spin != nullptr);

        model.transmitModel().setAntiVoxGainDb(15);
        QCoreApplication::processEvents();

        QCOMPARE(spin->value(), 15);
    }

    void gainSpinbox_tooltipMatchesThetisVerbatim()
    {
        RadioModel model;
        DexpVoxPage page(&model);
        page.show();

        auto* spin = page.findChild<QSpinBox*>(QStringLiteral("udAntiVoxGain"));
        QVERIFY(spin != nullptr);
        // From Thetis setup.designer.cs:44722 [v2.10.3.13].
        QCOMPARE(spin->toolTip(),
                 QStringLiteral("Gain applied to Anti-VOX audio (Anti-VOX sensitivity)"));
    }
};

QTEST_MAIN(TestDexpVoxSetupPage)
#include "tst_dexp_vox_setup_page.moc"
