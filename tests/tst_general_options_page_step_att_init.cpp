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
//
// Modification history (NereusSDR):
//   2026-09-29 : Different-band transmit matches Thetis (JJ's ruling) by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code: the enabled tooltip names the active slice, and a
//                 Core older than transmitSettingsVersion 14 shows the box
//                 disabled with its reason.

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/StepAttenuatorController.h"
#include "core/session/IStationLink.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/Band.h"
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

// A link to a Core that offers transmit settings up to `version`, to a
// device it permits to transmit.
class TransmitSettingsLink final : public IStationLink {
public:
    int version{0};
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    bool stationLinkReady() const override { return true; }
    bool transmitSettingsAvailable(int minVersion) const override { return version >= minVersion; }
    bool transmitSettingsPermitted() const override { return true; }
};

}  // namespace

class TestGeneralOptionsPageStepAttInit : public QObject
{
    Q_OBJECT

private slots:
    void regionEditsEffectivePolicyAndWaitsForReceive()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Region"), QStringLiteral("Italy"));
        settings.setValue(QStringLiteral("BandPlanRegion"), QStringLiteral("8"));
        RadioModel model;
        GeneralOptionsPage page(&model);
        auto* region = page.findChild<QComboBox*>(QStringLiteral("comboFRSRegion"));
        QVERIFY(region);
        QVERIFY(region->isEnabled());
        for (int id = 0; id < 24; ++id) {
            region->setCurrentIndex(id);
            QCOMPARE(settings.value(QStringLiteral("BandPlanRegion")).toInt(), id);
        }
        QCOMPARE(settings.value(QStringLiteral("Region")).toString(), QStringLiteral("Italy"));
        model.transmitModel().setMox(true);
        QVERIFY(!region->isEnabled());
        region->setCurrentIndex(3); // A stale/programmatic edit is refused too.
        QCOMPARE(settings.value(QStringLiteral("BandPlanRegion")).toInt(), 23);
        QCOMPARE(region->currentIndex(), 23);
        model.transmitModel().setMox(false);
        QVERIFY(region->isEnabled());
        region->setCurrentIndex(8);
        QCOMPARE(settings.value(QStringLiteral("BandPlanRegion")).toInt(), 8);
    }

    // Addendum G-42 (JJ's ruling 2026-09-28): Extended shows and changes
    // the Core's ExtendedTransmit (this computer's own radio here). An old
    // saved ExtendedTxAllowed neither shows nor is changed, and the box
    // waits for receive.
    void extendedIsTheCoresSettingAndWaitsForReceive()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("ExtendedTxAllowed"), QStringLiteral("True"));
        settings.remove(QStringLiteral("ExtendedTransmit"));
        RadioModel model;
        GeneralOptionsPage page(&model);
        auto* extended = page.findChild<QCheckBox*>(QStringLiteral("chkExtended"));
        QVERIFY(extended);
        QVERIFY(!extended->isChecked());
        QVERIFY(extended->isEnabled());
        QCOMPARE(extended->toolTip(), QStringLiteral("Enable extended TX (out of band)"));
        extended->setChecked(true);
        QCOMPARE(settings.value(QStringLiteral("ExtendedTransmit")).toString(), QStringLiteral("True"));
        QCOMPARE(settings.value(QStringLiteral("ExtendedTxAllowed")).toString(), QStringLiteral("True"));

        model.transmitModel().setMox(true);
        QVERIFY(!extended->isEnabled());
        QCOMPARE(extended->toolTip(), RadioModel::onAirReason());
        extended->setChecked(false); // A stale/programmatic edit is refused too.
        QCOMPARE(settings.value(QStringLiteral("ExtendedTransmit")).toString(), QStringLiteral("True"));
        QVERIFY(extended->isChecked());
        model.transmitModel().setMox(false);
        QVERIFY(extended->isEnabled());
        extended->setChecked(false);
        QCOMPARE(settings.value(QStringLiteral("ExtendedTransmit")).toString(), QStringLiteral("False"));

        // Another page on the same model (the Core's own window) follows a
        // change a device made on the Core.
        settings.setValue(QStringLiteral("ExtendedTransmit"), QStringLiteral("True"));
        model.reportTransmitGateSettingChanged(QStringLiteral("ExtendedTransmit"));
        QVERIFY(extended->isChecked());
        settings.remove(QStringLiteral("ExtendedTransmit"));
        settings.remove(QStringLiteral("ExtendedTxAllowed"));
    }

    // The three IARU entries read "IARU Region 1/2/3" (display text only).
    // The saved BandPlanRegion value stays the entry's number, so a value
    // saved before the rename still selects the same entry, and picking one
    // saves the same number as before.
    void iaruRegionsShowTheirNameAndKeepTheSavedNumber()
    {
        auto& settings = AppSettings::instance();
        const QStringList names{QStringLiteral("IARU Region 1"),
                                QStringLiteral("IARU Region 2"),
                                QStringLiteral("IARU Region 3")};
        for (int i = 0; i < names.size(); ++i) {
            const int saved = 20 + i;
            settings.setValue(QStringLiteral("BandPlanRegion"), QString::number(saved));
            RadioModel model;
            GeneralOptionsPage page(&model);
            auto* region = page.findChild<QComboBox*>(QStringLiteral("comboFRSRegion"));
            QVERIFY(region);
            QCOMPARE(region->count(), 24);
            QCOMPARE(region->itemText(saved), names.at(i));
            QCOMPARE(region->currentIndex(), saved);
            QCOMPARE(region->currentText(), names.at(i));
            region->setCurrentIndex(8);
            region->setCurrentIndex(saved);
            QCOMPARE(settings.value(QStringLiteral("BandPlanRegion")).toString(),
                     QString::number(saved));
        }
        settings.setValue(QStringLiteral("BandPlanRegion"), QStringLiteral("8"));
    }

    // JJ's ruling (2026-09-29): Prevent TX'ing on a different band is the
    // Core's PreventTxOnDifferentBandToRx, shown (never hidden), changed
    // only off the air, and followed when a device changes it on the Core.
    // The key name is Thetis's own, so this Core's saved value carries on.
    void preventDifferentBandIsTheCoresSettingAndWaitsForReceive()
    {
        auto& settings = AppSettings::instance();
        const QString key = QStringLiteral("PreventTxOnDifferentBandToRx");
        settings.setValue(key, QStringLiteral("True"));
        RadioModel model;
        GeneralOptionsPage page(&model);
        auto* prevent = page.findChild<QCheckBox*>(QStringLiteral("chkPreventTXonDifferentBandToRX"));
        QVERIFY(prevent);
        QVERIFY(!prevent->isHidden());
        QCOMPARE(prevent->property("nereusSetupId").toString(),
                 QStringLiteral("general.options.preventDifferentBand"));
        QVERIFY(prevent->isChecked());
        QVERIFY(prevent->isEnabled());
        QCOMPARE(prevent->toolTip(), QStringLiteral(
            "Refuse to transmit when the transmitting slice is not your active slice and "
            "is on a different band from it"));
        prevent->setChecked(false);
        QCOMPARE(settings.value(key).toString(), QStringLiteral("False"));

        model.transmitModel().setMox(true);
        QVERIFY(!prevent->isEnabled());
        QCOMPARE(prevent->toolTip(), RadioModel::onAirReason());
        prevent->setChecked(true); // A stale/programmatic edit is refused too.
        QCOMPARE(settings.value(key).toString(), QStringLiteral("False"));
        QVERIFY(!prevent->isChecked());
        model.transmitModel().setMox(false);
        QVERIFY(prevent->isEnabled());
        prevent->setChecked(true);
        QCOMPARE(settings.value(key).toString(), QStringLiteral("True"));

        // The Core's own window follows a change a device made on the Core.
        settings.setValue(key, QStringLiteral("False"));
        model.reportTransmitGateSettingChanged(key);
        QVERIFY(!prevent->isChecked());
        settings.remove(key);
    }

    // A remote window on a Core older than transmitSettingsVersion 14 shows
    // the box disabled with why (disabled, never hidden); a Core with it
    // makes the box usable.
    void preventDifferentBandOnAnOlderCoreSaysWhy()
    {
        RadioModel model(RadioModel::Role::Remote);
        TransmitSettingsLink link;
        link.version = 13;
        model.attachStation(&link);
        {
            GeneralOptionsPage page(&model);
            auto* prevent = page.findChild<QCheckBox*>(
                QStringLiteral("chkPreventTXonDifferentBandToRX"));
            QVERIFY(prevent);
            page.setStationSettingsAvailable(true, QString());
            QVERIFY(!prevent->isHidden());
            QVERIFY(!prevent->isEnabled());
            const QString olderCore = QStringLiteral(
                "This Core does not have Prevent transmitting on a different band. Update "
                "the Core to use it.");
            QCOMPARE(prevent->toolTip(), olderCore);
            QCOMPARE(prevent->accessibleDescription(), olderCore);

            link.version = 14;
            page.setStationSettingsAvailable(true, QString());
            QVERIFY(prevent->isEnabled());
            QVERIFY(prevent->accessibleDescription().isEmpty());
        }
        model.detachStation();
    }

    void invalidStoredRegionNeedsAnExplicitSelection()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("BandPlanRegion"), QStringLiteral("invalid"));
        RadioModel model;
        GeneralOptionsPage page(&model);
        auto* region = page.findChild<QComboBox*>(QStringLiteral("comboFRSRegion"));
        QVERIFY(region);
        QCOMPARE(region->currentIndex(), -1);
        QCOMPARE(settings.value(QStringLiteral("BandPlanRegion")).toString(), QStringLiteral("invalid"));
        region->setCurrentIndex(8);
        QCOMPARE(settings.value(QStringLiteral("BandPlanRegion")).toString(), QStringLiteral("8"));
    }

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    // R-R3-46 / R-R3-11: the RX2 row shows and sets the other ADC's own
    // attenuator (Thetis udRX2StepAttData) on a two-ADC radio. Its Enable
    // is RX1's (one enable in NereusSDR), shown disabled with the reason.
    void rx2RowShowsAndSetsTheOtherAdcsAttenuator()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        GeneralOptionsPage* page = makePageWithController(model, true, 5, nullptr);
        StepAttenuatorController* ctrl = model.stepAttController();
        ctrl->setRx2Attenuation(12);
        auto* spin = page->findChild<QSpinBox*>(QStringLiteral("spnRx2StepAttValue"));
        auto* enable = page->findChild<QCheckBox*>(QStringLiteral("chkRx2StepAttEnable"));
        QVERIFY(spin && enable);
        QVERIFY(spin->isVisibleTo(page));
        QVERIFY(enable->isVisibleTo(page));
        QCOMPARE(spin->property("nereusSetupId").toString(),
                 QStringLiteral("general.options.rx2StepAtt"));
        QVERIFY(spin->isEnabled());
        QCOMPARE(spin->value(), 12);
        spin->setValue(7);
        QCOMPARE(ctrl->rx2AttenuatorDb(), 7);
        QCOMPARE(ctrl->attenuatorDb(), 5);
        ctrl->setRx2Attenuation(3);
        QCOMPARE(spin->value(), 3);
        // RX2's own enable (Thetis chkRX2StepAtt, _rx2_step_att_enabled).
        QVERIFY(enable->isEnabled());
        QVERIFY(enable->isChecked());
        QCOMPARE(enable->property("nereusSetupId").toString(),
                 QStringLiteral("general.options.rx2StepAttEnable"));
        ctrl->setAdcRouting(0, 1, Band::Band20m, false);  // RX2 on its own ADC
        enable->click();
        QVERIFY(!ctrl->rx2StepAttEnabled());
        QVERIFY(ctrl->stepAttEnabled());
        QVERIFY(!spin->isEnabled());
        ctrl->setRx2StepAttEnabled(true);
        QVERIFY(enable->isChecked());
        QVERIFY(spin->isEnabled());

        // Auto Attenuate RX2: its own Enable, Undo and Hold, as Thetis's
        // chkAutoATTRx2, chkAutoAttUndoRX2 and nudAutoAttHoldRX2 (no mode).
        auto* rx2Auto = page->findChild<QGroupBox*>(QStringLiteral("grpAutoAttRx2"));
        QVERIFY(rx2Auto);
        QVERIFY(rx2Auto->isEnabled());
        QVERIFY(rx2Auto->findChildren<QComboBox*>().isEmpty());
        QCheckBox* autoEnable = nullptr;
        QCheckBox* autoUndo = nullptr;
        for (QCheckBox* c : rx2Auto->findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Enable")) { autoEnable = c; }
            if (c->text() == QStringLiteral("Undo")) { autoUndo = c; }
        }
        auto* hold = rx2Auto->findChild<QSpinBox*>();
        QVERIFY(autoEnable && autoUndo && hold);
        autoEnable->click();
        QVERIFY(ctrl->rx2AutoAttEnabled());
        QVERIFY(!ctrl->autoAttEnabled());
        QVERIFY(autoUndo->isEnabled());
        autoUndo->click();
        QVERIFY(ctrl->rx2AutoAttUndo());
        QVERIFY(hold->isEnabled());
        hold->setValue(8);
        QCOMPARE(ctrl->rx2AutoUndoDelaySec(), 8);
        ctrl->setRx2AutoAttEnabled(false);
        QVERIFY(!autoEnable->isChecked());
        delete page;
    }

    // One receive ADC: the row is shown, disabled, with the plain reason.
    void rx2RowIsDisabledOnAOneAdcRadio()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        GeneralOptionsPage* page = makePageWithController(model, true, 5, nullptr);
        auto* spin = page->findChild<QSpinBox*>(QStringLiteral("spnRx2StepAttValue"));
        QVERIFY(spin);
        QVERIFY(spin->isVisibleTo(page));
        QVERIFY(!spin->isEnabled());
        QVERIFY(!spin->toolTip().isEmpty());
        auto* rx2Enable = page->findChild<QCheckBox*>(QStringLiteral("chkRx2StepAttEnable"));
        QVERIFY(rx2Enable);
        QVERIFY(rx2Enable->isVisibleTo(page));
        QVERIFY(!rx2Enable->isEnabled());
        QVERIFY(!rx2Enable->toolTip().isEmpty());
        // Auto Attenuate RX2 is shown too, disabled: there is no RX2 ADC.
        auto* rx2Auto = page->findChild<QGroupBox*>(QStringLiteral("grpAutoAttRx2"));
        QVERIFY(rx2Auto);
        QVERIFY(rx2Auto->isVisibleTo(page));
        QVERIFY(!rx2Auto->isEnabled());
        QVERIFY(!rx2Auto->toolTip().isEmpty());
        delete page;
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

    // ── RX2 row and Auto Attenuate RX2 are shown, never hidden. ─────────────
    //
    // R-R3-46 / R-R3-11: issue #259 hid them until the controller held RX2's
    // own attenuator; it does now (rx2AttenuatorDb). RX2 Enable follows RX1
    // Enable (one enable in NereusSDR) and Auto Attenuate RX2 runs on RX1's
    // settings, so both are shown disabled with the reason ("disabled,
    // never hidden"). rx2RowShowsAndSetsTheOtherAdcsAttenuator covers the
    // value itself.
    void rx2RowAndAutoAttRx2AreShownDisabled()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
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
        QVERIFY(!rx2Chk->isHidden());
        QVERIFY(!rx2Chk->isEnabled());
        QVERIFY(!rx2Chk->toolTip().isEmpty());

        QGroupBox* autoAttRx2 = nullptr;
        QGroupBox* autoAttRx1 = nullptr;
        for (auto* g : page->findChildren<QGroupBox*>()) {
            if (g->title() == QStringLiteral("Auto Attenuate RX2")) {
                autoAttRx2 = g;
            } else if (g->title() == QStringLiteral("Auto Attenuate RX1")) {
                autoAttRx1 = g;
            }
        }
        QVERIFY2(autoAttRx2 && autoAttRx1, "Auto Attenuate groupboxes not found");
        QVERIFY(!autoAttRx2->isHidden());
        QVERIFY(!autoAttRx2->isEnabled());
        QVERIFY(!autoAttRx2->toolTip().isEmpty());
        QVERIFY(!autoAttRx1->isHidden());
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
