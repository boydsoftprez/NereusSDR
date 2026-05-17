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
};

QTEST_MAIN(TestGeneralOptionsPageStepAttInit)
#include "tst_general_options_page_step_att_init.moc"
