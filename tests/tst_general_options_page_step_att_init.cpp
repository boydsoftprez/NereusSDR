// tests/tst_general_options_page_step_att_init.cpp  (NereusSDR)
//
// no-port-check: test fixture — no Thetis attribution required.
//
// Issue #259 regression — GeneralOptionsPage was constructed lazily on every
// Tools → Setup open via `new SetupDialog(m_radioModel, this)` (seven call
// sites in MainWindow.cpp). On the post-restart open the page therefore
// missed the load-time stepAttEnabledChanged + attenuationChanged signals
// fired by StepAttenuatorController::loadSettings during connectToRadio,
// so the "RX1 Enable" checkbox and "RX1 dB" spinbox displayed their
// constructor defaults (unchecked / 0) instead of the persisted state.
//
// The fix adds a single initFromController() call at the end of the page
// constructor that pulls m_ctrl->stepAttEnabled() and m_ctrl->attenuatorDb()
// into the widgets (signals blocked so the read does not loop back to the
// controller). These tests pin that behaviour.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "core/StepAttenuatorController.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

// Construct a real StepAttenuatorController, install it on the model with
// the requested initial enable/value state, then build the page on top.
// Lifetime: the controller is parented to the model so it follows the
// model's QObject teardown.
GeneralOptionsPage* makePageWithController(RadioModel& model,
                                           bool stepAttEnabled,
                                           int  attDb,
                                           QObject* parent)
{
    auto* ctrl = new StepAttenuatorController(&model);
    ctrl->setMaxAttenuation(31);  // ANAN-10E classic range
    ctrl->setStepAttEnabled(stepAttEnabled);
    ctrl->setAttenuation(attDb, 0);
    model.setStepAttController(ctrl);

    auto* page = new GeneralOptionsPage(&model, qobject_cast<QWidget*>(parent));
    return page;
}

}  // namespace

class TestGeneralOptionsPageStepAttInit : public QObject
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

    // ── Controller has restored state BEFORE the page is constructed. ────────
    //
    // Mirrors the post-restart timeline:
    //   1. App launches.
    //   2. RadioModel connects → controller's loadSettings runs and restores
    //      m_stepAttEnabled=true, m_attDb=5.
    //   3. User opens Setup → page is constructed NOW (after the load).
    //   4. Checkbox + spinbox must reflect the restored state.
    //
    // Before the fix the page would show unchecked / 0 because it relied on
    // a stepAttEnabledChanged signal that had already fired.
    void initFromController_restoresEnabledAndValue()
    {
        RadioModel model;
        auto* page = makePageWithController(model, /*enabled=*/true,
                                            /*dB=*/5, this);

        auto* chk = page->findChild<QCheckBox*>();
        Q_UNUSED(chk);
        // Resolve the specific RX1 widgets by walking children — the
        // GeneralOptionsPage holds them as private members so we can't
        // address them directly. The checkbox text is "RX1 Enable" and
        // the spinbox is the only one inside the same Step Attenuator
        // group, so we find by text / sibling.

        QCheckBox* rx1Chk = nullptr;
        for (auto* c : page->findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("RX1 Enable")) {
                rx1Chk = c;
                break;
            }
        }
        QVERIFY2(rx1Chk, "RX1 Enable checkbox not found");
        QCOMPARE(rx1Chk->isChecked(), true);

        // The RX1 spinbox sits next to the RX1 checkbox inside the Step
        // Attenuator group. The group's QSpinBox at index 0 (after layout
        // construction) is the RX1 one.
        auto spinBoxes = page->findChildren<QSpinBox*>();
        QVERIFY2(!spinBoxes.isEmpty(), "no QSpinBox children");
        QSpinBox* rx1Spin = nullptr;
        for (auto* s : spinBoxes) {
            // Match by enabled state cascade — the RX1 spinbox is the one
            // sibling of the RX1 checkbox we just found, so it has the
            // same parent QWidget (the layout's enclosing QWidget).
            if (s->parent() == rx1Chk->parent()) {
                rx1Spin = s;
                break;
            }
        }
        QVERIFY2(rx1Spin, "RX1 dB spinbox not found");
        QCOMPARE(rx1Spin->value(), 5);
        QCOMPARE(rx1Spin->isEnabled(), true);
    }

    // ── Controller has enable=false → checkbox unchecked, spinbox disabled. ──
    void initFromController_disabledState()
    {
        RadioModel model;
        auto* page = makePageWithController(model, /*enabled=*/false,
                                            /*dB=*/12, this);

        QCheckBox* rx1Chk = nullptr;
        for (auto* c : page->findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("RX1 Enable")) {
                rx1Chk = c;
                break;
            }
        }
        QVERIFY2(rx1Chk, "RX1 Enable checkbox not found");
        QCOMPARE(rx1Chk->isChecked(), false);

        QSpinBox* rx1Spin = nullptr;
        for (auto* s : page->findChildren<QSpinBox*>()) {
            if (s->parent() == rx1Chk->parent()) {
                rx1Spin = s;
                break;
            }
        }
        QVERIFY2(rx1Spin, "RX1 dB spinbox not found");
        // Spinbox value should still be the controller's m_attDb (12),
        // but the spinbox itself must be disabled because the enable
        // checkbox is off.
        QCOMPARE(rx1Spin->value(), 12);
        QCOMPARE(rx1Spin->isEnabled(), false);
    }

    // ── RX2 row is hidden until independent RX2 state lands. ─────────────────
    //
    // Pins the deliberate UI gate: the RX2 step-att controls exist in the
    // widget tree (so the existing connectController() / layout code keeps
    // working) but are not visible. Lift this assertion when the controller
    // grows an m_stepAttEnabledRx2 / m_attDbRx2 pair plus a click-time
    // RX1↔RX2 mirror per Thetis setup.cs:15741-15760 [v2.10.3.13].
    void rx2Row_isHiddenForNow()
    {
        RadioModel model;
        auto* page = makePageWithController(model, /*enabled=*/true,
                                            /*dB=*/5, this);

        QCheckBox* rx2Chk = nullptr;
        for (auto* c : page->findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("RX2 Enable")) {
                rx2Chk = c;
                break;
            }
        }
        QVERIFY2(rx2Chk, "RX2 Enable checkbox not found");
        QVERIFY2(rx2Chk->isHidden(),
                 "RX2 Enable must be hidden until independent RX2 state lands");
    }

    // ── Auto-Att RX2 group is hidden until independent RX2 state lands. ──────
    //
    // Pins the same gate as rx2Row_isHiddenForNow but for the second
    // groupbox built by buildAutoAttGroup. The controller is single-RX
    // (auto-att RX2 is future expansion); shipping the box would imply
    // independent control we don't yet store. From Thetis groupBoxTS47 the
    // RX1 / RX2 auto-att boxes are independent — same Phase 3F follow-up.
    void autoAttRx2Group_isHiddenForNow()
    {
        RadioModel model;
        auto* page = makePageWithController(model, /*enabled=*/true,
                                            /*dB=*/5, this);

        QGroupBox* autoAttRx2 = nullptr;
        for (auto* g : page->findChildren<QGroupBox*>()) {
            if (g->title() == QStringLiteral("Auto Attenuate RX2")) {
                autoAttRx2 = g;
                break;
            }
        }
        QVERIFY2(autoAttRx2, "Auto Attenuate RX2 groupbox not found");
        QVERIFY2(autoAttRx2->isHidden(),
                 "Auto Attenuate RX2 must be hidden until independent RX2 state lands");

        // Sanity check: RX1 auto-att group remains visible (not hidden).
        QGroupBox* autoAttRx1 = nullptr;
        for (auto* g : page->findChildren<QGroupBox*>()) {
            if (g->title() == QStringLiteral("Auto Attenuate RX1")) {
                autoAttRx1 = g;
                break;
            }
        }
        QVERIFY2(autoAttRx1, "Auto Attenuate RX1 groupbox not found");
        QVERIFY2(!autoAttRx1->isHidden(),
                 "Auto Attenuate RX1 must remain visible");
    }

    // ── Auto-Att RX1: full cold-open restore (PR #260 review fix). ───────────
    //
    // The first pass of initFromController only pulled autoAttEnabled +
    // autoAttMode. The reviewer flagged that the Undo/Decay checkbox and
    // the Hold/Delay spinbox stayed at constructor defaults even when the
    // controller had restored real values from disk. This test pins the
    // expanded init so every Auto-Att RX1 widget reflects the controller.
    //
    // Adaptive mode + autoUndoEnabled=true + adaptiveHoldMs=4000:
    //   - cmbAutoAttRx1Mode → "Adaptive"
    //   - chkAutoAttUndoRx1: checked, label "Decay" (Adaptive renames Undo
    //     → Decay per buildAutoAttGroup line ~596).
    //   - spnAutoAttHoldRx1: 4 seconds, enabled (autoOn && chkUndo).
    void initFromController_restoresAutoAttAdaptive()
    {
        RadioModel model;
        auto* ctrl = new StepAttenuatorController(&model);
        ctrl->setMaxAttenuation(31);
        ctrl->setStepAttEnabled(true);
        ctrl->setHasStepAttenuatorCal(true);  // gate Adaptive on
        ctrl->setAutoAttEnabled(true);
        ctrl->setAutoAttMode(AutoAttMode::Adaptive);
        ctrl->setAutoAttUndo(true);
        ctrl->setAutoAttHoldSeconds(4.0);    // 4s → 4000 ms
        model.setStepAttController(ctrl);

        auto* page = new GeneralOptionsPage(&model, qobject_cast<QWidget*>(this));

        // Find the Auto Attenuate RX1 group + its three widgets.
        QGroupBox* group = nullptr;
        for (auto* g : page->findChildren<QGroupBox*>()) {
            if (g->title() == QStringLiteral("Auto Attenuate RX1")) {
                group = g;
                break;
            }
        }
        QVERIFY2(group, "Auto Attenuate RX1 groupbox not found");

        QComboBox* mode  = group->findChild<QComboBox*>();
        QCheckBox* undo  = nullptr;
        QCheckBox* en    = nullptr;
        for (auto* c : group->findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Enable")) {
                en = c;
            } else {
                undo = c;  // the only other QCheckBox is the Undo/Decay one
            }
        }
        QSpinBox* hold = group->findChild<QSpinBox*>();
        QVERIFY2(en   && mode && undo && hold, "Auto-Att widgets missing");

        QCOMPARE(en->isChecked(),   true);
        QCOMPARE(mode->currentText(), QStringLiteral("Adaptive"));
        QCOMPARE(undo->isChecked(), true);
        QCOMPARE(undo->text(),      QStringLiteral("Decay"));
        QCOMPARE(hold->value(),     4);
        QCOMPARE(mode->isEnabled(), true);
        QCOMPARE(undo->isEnabled(), true);
        QCOMPARE(hold->isEnabled(), true);
    }

    // ── Auto-Att RX1: Classic mode pulls autoUndoDelaySec onto the spinbox. ──
    void initFromController_restoresAutoAttClassic()
    {
        RadioModel model;
        auto* ctrl = new StepAttenuatorController(&model);
        ctrl->setMaxAttenuation(31);
        ctrl->setStepAttEnabled(true);
        ctrl->setAutoAttEnabled(true);
        ctrl->setAutoAttMode(AutoAttMode::Classic);
        ctrl->setAutoAttUndo(true);
        ctrl->setAutoUndoDelaySec(9);
        model.setStepAttController(ctrl);

        auto* page = new GeneralOptionsPage(&model, qobject_cast<QWidget*>(this));

        QGroupBox* group = nullptr;
        for (auto* g : page->findChildren<QGroupBox*>()) {
            if (g->title() == QStringLiteral("Auto Attenuate RX1")) {
                group = g;
                break;
            }
        }
        QVERIFY2(group, "Auto Attenuate RX1 groupbox not found");

        QComboBox* mode = group->findChild<QComboBox*>();
        QCheckBox* undo = nullptr;
        for (auto* c : group->findChildren<QCheckBox*>()) {
            if (c->text() != QStringLiteral("Enable")) {
                undo = c;
                break;
            }
        }
        QSpinBox* hold = group->findChild<QSpinBox*>();
        QVERIFY(mode && undo && hold);

        QCOMPARE(mode->currentText(), QStringLiteral("Classic"));
        QCOMPARE(undo->isChecked(),   true);
        QCOMPARE(undo->text(),        QStringLiteral("Undo"));  // Classic keeps "Undo"
        QCOMPARE(hold->value(),       9);                       // delay seconds, not hold
    }
};

QTEST_MAIN(TestGeneralOptionsPageStepAttInit)
#include "tst_general_options_page_step_att_init.moc"
