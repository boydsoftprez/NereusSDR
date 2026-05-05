// no-port-check: NereusSDR-original tests for hermes-filter-debug Bug 2.
//
// Hl2IoBoardTab N2ADR-toggle behavioural tests:
//   * onN2adrToggled emits settingChanged("n2adrFilter", ...) — required so
//     HardwarePage::wire() routes the value to per-MAC AppSettings instead
//     of the previous direct global setValue (which broke restore on next
//     launch).
//   * onN2adrToggled mutates the OcMatrix as before (idempotent path).
//   * restoreSettings({}) leaves the OcMatrix ALONE when the persisted key
//     is absent — the previous behaviour was to call onN2adrToggled(false),
//     wiping a matrix that had been correctly populated at connect time.
//   * restoreSettings({"n2adrFilter":"True"}) re-applies the matrix without
//     re-emitting settingChanged (no echo back through HardwarePage's
//     onTabSettingChanged immediately after a read).
//
// hermes-filter-debug 2026-05-01 — JJ Boyd (KG4VCF), AI-assisted.

#include <QtTest/QtTest>
#include <QApplication>
#include <QtTest/QSignalSpy>

#include "models/Band.h"
#include "core/HpsdrModel.h"
#include "core/OcMatrix.h"
#include "core/accessories/N2adrPreset.h"
#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

// Returns true ⟺ at least one OC pin in the matrix is set across any band /
// any tx-or-rx column.  Proxy for "applyN2adrPreset actually ran" — the
// N2ADR preset writes 23 bits across 10 ham + 13 SWL bands.
bool ocMatrixHasAnyPinSet(const OcMatrix& oc)
{
    for (int b = 0; b < int(Band::Count); ++b) {
        const Band band = static_cast<Band>(b);
        if (oc.maskFor(band, /*tx=*/false) != 0 ||
            oc.maskFor(band, /*tx=*/true)  != 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

class TstHl2IoBoardTabN2adr : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // onN2adrToggled emits settingChanged so HardwarePage's wire() can
    // route to per-MAC AppSettings.  Verified by spying on the signal.
    // ─────────────────────────────────────────────────────────────────────
    void toggle_emits_settingChanged_for_per_mac_routing()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        QSignalSpy spy(&tab, &Hl2IoBoardTab::settingChanged);
        QVERIFY(spy.isValid());

        // Find the checkbox by object name and toggle programmatically.
        // (We don't expose m_n2adrFilter publicly; the slot is the public
        // surface.)
        tab.triggerN2adrToggleForTest(true);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("n2adrFilter"));
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("True"));
    }

    void toggle_off_emits_false()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);
        QSignalSpy spy(&tab, &Hl2IoBoardTab::settingChanged);

        tab.triggerN2adrToggleForTest(false);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("False"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // onN2adrToggled(true) populates the matrix; (false) wipes it.
    // ─────────────────────────────────────────────────────────────────────
    void toggle_mutates_oc_matrix()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        // Before: matrix empty.
        QVERIFY(!ocMatrixHasAnyPinSet(model.ocMatrix()));

        tab.triggerN2adrToggleForTest(true);
        QVERIFY(ocMatrixHasAnyPinSet(model.ocMatrix()));

        tab.triggerN2adrToggleForTest(false);
        QVERIFY(!ocMatrixHasAnyPinSet(model.ocMatrix()));
    }

    // ─────────────────────────────────────────────────────────────────────
    // THE CORE BUG-2 REGRESSION GUARD
    //
    // Pre-fix behaviour: opening Setup → HL2 IO tab on a fresh launch
    // (with the legacy bug — global written, per-MAC absent at restore
    // time) called onN2adrToggled(false) and WIPED the OcMatrix that had
    // been correctly populated at connect by RadioModel::connectToRadio.
    //
    // Post-fix: a missing "n2adrFilter" key in the per-MAC settings dict
    // means "no explicit user intent stored for this radio yet" — leave
    // the matrix alone.
    // ─────────────────────────────────────────────────────────────────────
    void restoreSettings_with_missing_key_does_NOT_wipe_matrix()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        // Simulate the connect-time matrix load by populating directly.
        applyN2adrPreset(model.ocMatrixMutable(), /*enabled=*/true);
        QVERIFY(ocMatrixHasAnyPinSet(model.ocMatrix()));

        // Restore with no n2adrFilter key (per-MAC has nothing yet).
        QMap<QString, QVariant> emptySettings;
        tab.restoreSettings(emptySettings);

        // Matrix MUST still be populated.
        QVERIFY2(ocMatrixHasAnyPinSet(model.ocMatrix()),
                 "restoreSettings with missing n2adrFilter key wiped the OcMatrix "
                 "— this is the hermes-filter-debug Bug 2 regression");
    }

    // ─────────────────────────────────────────────────────────────────────
    // restoreSettings with explicit "True" applies the preset to the matrix.
    // (Idempotent if it was already populated; populates if it wasn't.)
    // ─────────────────────────────────────────────────────────────────────
    void restoreSettings_with_true_populates_matrix()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        QVERIFY(!ocMatrixHasAnyPinSet(model.ocMatrix()));

        QMap<QString, QVariant> settings;
        settings.insert(QStringLiteral("n2adrFilter"),
                        QVariant(QStringLiteral("True")));
        tab.restoreSettings(settings);

        QVERIFY(ocMatrixHasAnyPinSet(model.ocMatrix()));
    }

    // ─────────────────────────────────────────────────────────────────────
    // restoreSettings with explicit "False" wipes the matrix (matches the
    // user's stored intent).
    // ─────────────────────────────────────────────────────────────────────
    void restoreSettings_with_false_wipes_matrix()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        applyN2adrPreset(model.ocMatrixMutable(), /*enabled=*/true);
        QVERIFY(ocMatrixHasAnyPinSet(model.ocMatrix()));

        QMap<QString, QVariant> settings;
        settings.insert(QStringLiteral("n2adrFilter"),
                        QVariant(QStringLiteral("False")));
        tab.restoreSettings(settings);

        QVERIFY(!ocMatrixHasAnyPinSet(model.ocMatrix()));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Codex P2 (PR #160 review) + Issue #174: when the key is absent, the
    // CHECKBOX must be reset to the default state.  The Hl2IoBoardTab
    // instance is reused across currentRadioChanged emissions; without
    // the reset, switching radios would show stale UI from the previously
    // selected radio, decoupled from the connect-time matrix state.
    //
    // Default flipped to TRUE in issue #174 — strict mi0bot port from
    // setup.designer.cs:17466-17467 [v2.10.3.13-beta2] chkHERCULES.Checked.
    // RadioModel::connectToRadio now reconciles the matrix to True on
    // absent key; the checkbox must match.
    // ─────────────────────────────────────────────────────────────────────
    void restoreSettings_with_missing_key_resets_checkbox_to_default_true()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);

        // Simulate a previously-selected HL2 having left the box unchecked.
        tab.setN2adrCheckboxStateForTest(false);
        QVERIFY(!tab.n2adrCheckboxStateForTest());

        // Switch to an HL2 with no persisted N2ADR key.
        QMap<QString, QVariant> emptySettings;
        tab.restoreSettings(emptySettings);

        QVERIFY2(tab.n2adrCheckboxStateForTest(),
                 "Issue #174: restoreSettings with missing key did not "
                 "reset the checkbox to mi0bot default True — checkbox "
                 "would be out of sync with the connect-time matrix");
    }

    // ─────────────────────────────────────────────────────────────────────
    // restoreSettings does NOT echo settingChanged back to HardwarePage —
    // that would create a redundant write through onTabSettingChanged
    // immediately after a read. The signal is for user-driven toggles
    // only; restore goes through applyN2adrMatrix directly.
    // ─────────────────────────────────────────────────────────────────────
    void restoreSettings_does_not_emit_settingChanged()
    {
        RadioModel model;
#ifdef NEREUS_BUILD_TESTS
        model.setBoardForTest(HPSDRHW::HermesLite);
#endif
        Hl2IoBoardTab tab(&model);
        QSignalSpy spy(&tab, &Hl2IoBoardTab::settingChanged);

        QMap<QString, QVariant> settings;
        settings.insert(QStringLiteral("n2adrFilter"),
                        QVariant(QStringLiteral("True")));
        tab.restoreSettings(settings);

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_APPLESS_MAIN(TstHl2IoBoardTabN2adr)
#include "tst_hl2_io_board_tab_n2adr.moc"
