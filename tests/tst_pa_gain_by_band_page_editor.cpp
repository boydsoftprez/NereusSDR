// tests/tst_pa_gain_by_band_page_editor.cpp  (NereusSDR)
//
// Phase 6 of issue #167 PA-cal safety hotfix.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies the live PaGainByBandPage editor (replaces the Phase 2 placeholder
// body in PaSetupPages.cpp:117-145).  The editor surface mirrors Thetis
// tpGainByBand (setup.designer.cs:47386-47525 [v2.10.3.13]) plus
// chkAutoPACalibrate (setup.designer.cs:49084 [v2.10.3.13]):
//
//   - profile combo (comboPAProfile)
//   - 4 lifecycle buttons (New / Copy / Delete / Reset Defaults)
//   - 14-band gain spinbox grid (nud160M..nudVHF13)
//   - 14x9 drive-step adjust spinbox matrix (panelAdjustGain — NereusSDR
//     densification: Thetis ships only the row for the selected band)
//   - per-band max-power column (nudMaxPowerForBandPA + chkUsePowerOnDrvTunPA)
//   - warning icon + label (pbPAProfileWarning + lblPAProfileWarning)
//   - chkPANewCal "New Cal" mode toggle
//   - chkAutoPACalibrate auto-cal toggle (Phase 7 wires the sweep flow)
//
// Test seams (always-on, gated by NEREUS_BUILD_TESTS to keep the QObject
// surface identical between builds):
//   - profileComboForTest()       — return the QComboBox*
//   - gainSpinForTest(Band)       — return the per-band gain QDoubleSpinBox*
//   - adjustSpinForTest(Band,int) — return the [band, step] adjust spinbox
//   - maxPowerSpinForTest(Band)   — return the per-band max-power spinbox
//   - useMaxPowerCheckForTest(Band) — return the per-band use-max checkbox
//   - autoCalibrateCheckForTest() — return the chkAutoPACalibrate
//   - newCalCheckForTest()        — return chkPAValues "New Cal" checkbox
//   - newButtonForTest() / copyButtonForTest() / deleteButtonForTest()
//   - resetButtonForTest()
//   - warningIconForTest() / warningLabelForTest()
//   - setNextProfileNameForTest(QString) — bypass QInputDialog::getText
//   - setDeleteConfirmedForTest(bool)    — bypass QMessageBox confirm
//   - setResetConfirmedForTest(bool)     — bypass QMessageBox confirm

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "gui/setup/PaSetupPages.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

inline QString testMac() { return QStringLiteral("AA:BB:CC:DD:EE:6A"); }

// Helper: prime a model + PaProfileManager so the page has live data to
// render on construction.
//
// RadioModel constructs a PaProfileManager in its ctor (Phase 4A wiring).
// Tests scope it directly via setMacAddress + load() since RadioModel itself
// is not yet a connect-driver in this fixture (no live UDP / discovery).
void primeModelWithProfiles(RadioModel& model)
{
    auto* mgr = model.paProfileManager();
    Q_ASSERT(mgr != nullptr);  // RadioModel must construct PaProfileManager
    mgr->setMacAddress(testMac());
    mgr->load(HPSDRModel::ANAN8000D);
    // #202 deep-fix: PaGainByBandPage's combo filter (userVisibleProfileNames)
    // reads RadioModel::hardwareProfile().model to decide which factory
    // default is visible.  Sync the test seam so the page sees ANAN8000D
    // as the connected model (matching PaProfileManager::load(ANAN8000D)
    // above).
    model.setHpsdrModelForTest(HPSDRModel::ANAN8000D);
}

}  // namespace

class TstPaGainByBandPageEditor : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    void init()
    {
        // Each test starts with a clean AppSettings so per-MAC state from the
        // previous test does not leak.
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    void combo_populates_from_profile_manager();
    void selecting_profile_loads_gain_spinbox_values();
    void editing_gain_spinbox_saves_into_profile_manager();
    void editing_adjust_spinbox_saves_into_profile_manager();
    void editing_max_power_spinbox_saves_into_profile_manager();
    void toggling_use_max_power_checkbox_saves_into_profile_manager();
    void new_button_creates_profile_and_sets_active();
    void copy_button_copies_active_profile_with_custom_values();
    void delete_button_refuses_last_remaining_profile();
    void delete_button_removes_other_profile();
    void reset_defaults_regenerates_canonical_values();
    void auto_cal_checkbox_present_and_toggleable();
    void warning_label_visible_when_profile_diverges();
    void gain_spinbox_bounds_match_thetis_minimum_38_8();  // issue #199
};

// ---------------------------------------------------------------------------
// 1. Combo populates from PaProfileManager filtered profile names.
//
//    Issue #202 deep-fix: combo is filtered via
//    PaProfileManager::userVisibleProfileNames so the user sees:
//      - exactly one factory-default entry: `Default - <connectedModel>`
//      - all user-customised (non-default) profiles
//    Other `Default - *` entries are hidden — mirrors Thetis's filter at
//    setup.cs:23341-23344 [v2.10.3.13].  PaProfileManager seeds 16 factory
//    rows + Bypass, but only 1 (`Default - ANAN8000D` for the test radio)
//    is visible to the user.  Bypass is also a factory-default and gets
//    hidden when the connected model isn't FIRST — matches Thetis where
//    Bypass appears only when explicitly selected (setup.cs:23335-23339).
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::combo_populates_from_profile_manager()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* combo = page.profileComboForTest();
    QVERIFY2(combo != nullptr, "PaGainByBandPage must own a QComboBox profile selector");

    // Only `Default - ANAN8000D` is visible (Bypass is HPSDRModel::FIRST,
    // not the connected model, so it's filtered out unless currently
    // selected per the Thetis-faithful Bypass-mode special case).
    QCOMPARE(combo->count(), 1);
    QVERIFY(combo->findText(QStringLiteral("Default - ANAN8000D")) >= 0);

    // Active profile (set by load(ANAN8000D)) must be the current selection.
    QCOMPARE(combo->currentText(), QStringLiteral("Default - ANAN8000D"));
}

// ---------------------------------------------------------------------------
// 2. Selecting Bypass via the combo loads the Hermes 41.x dB row.
//
//    Issue #202 deep-fix: Bypass profile no longer uses the all-100.0f
//    sentinel.  Per Thetis setup.cs:23314 [v2.10.3.13] Bypass is constructed
//    with HPSDRModel.FIRST, which falls through to the FIRST/HERMES/HPSDR/
//    ORIONMKII shared 41.x dB row at clsHardwareSpecific.cs:471-486
//    [v2.10.3.13].  Switching to Bypass must show those values on the
//    HF spinboxes.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::selecting_profile_loads_gain_spinbox_values()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* combo = page.profileComboForTest();
    QVERIFY(combo != nullptr);

    // Selecting "Bypass" via setCurrentText only works when it's in the
    // combo's items.  Push it through the manager: setActiveProfile fires
    // activeProfileChanged which rebuilds the combo with the Bypass-mode
    // special case (only Bypass visible — Thetis setup.cs:23335-23339
    // [v2.10.3.13]).
    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);
    mgr->setActiveProfile(QStringLiteral("Bypass"));

    // Hermes 41.x dB row from clsHardwareSpecific.cs:475-485 [v2.10.3.13]:
    //   B160M=41.0, B80M=41.2, B60M=41.3, B40M=41.3, B30M=41.0,
    //   B20M=40.5,  B17M=39.9, B15M=38.8, B12M=38.8, B10M=38.8, B6M=38.8
    QCOMPARE(page.gainSpinForTest(Band::Band160m)->value(), 41.0);
    QCOMPARE(page.gainSpinForTest(Band::Band80m)->value(),  41.2);
    QCOMPARE(page.gainSpinForTest(Band::Band20m)->value(),  40.5);
    QCOMPARE(page.gainSpinForTest(Band::Band6m)->value(),   38.8);
    // GEN/WWV/XVTR retain the kPaGainSentinel = 100.0f (no Thetis
    // equivalent in the gain table; lookupHfBand fall-through).
    QCOMPARE(page.gainSpinForTest(Band::GEN)->value(),  100.0);
    QCOMPARE(page.gainSpinForTest(Band::WWV)->value(),  100.0);
    QCOMPARE(page.gainSpinForTest(Band::XVTR)->value(), 100.0);
}

// ---------------------------------------------------------------------------
// 3. Editing a per-band gain spinbox saves into PaProfileManager. Verifies
//    the round-trip: spinbox edit -> active profile mutated -> profile
//    manager picks up the new value via profileByName().
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::editing_gain_spinbox_saves_into_profile_manager()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* spin = page.gainSpinForTest(Band::Band80m);
    QVERIFY(spin != nullptr);

    spin->setValue(49.0);

    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);

    const PaProfile* active = mgr->activeProfile();
    QVERIFY2(active != nullptr, "Active profile must be set after load()");
    QCOMPARE(active->getGainForBand(Band::Band80m, /*driveValue=*/-1),
             49.0f);
}

// ---------------------------------------------------------------------------
// 4. Editing a drive-step adjust spinbox saves into PaProfileManager. Tests
//    the 14x9 grid surface: pick band 80m at step 5 (drive % 60), set 0.5,
//    verify the active profile reflects.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::editing_adjust_spinbox_saves_into_profile_manager()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* spin = page.adjustSpinForTest(Band::Band80m, /*step=*/5);
    QVERIFY2(spin != nullptr, "Adjust spinbox at [80m, step 5] must exist");

    spin->setValue(0.5);

    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);

    const PaProfile* active = mgr->activeProfile();
    QVERIFY(active != nullptr);
    QCOMPARE(active->getAdjust(Band::Band80m, 5), 0.5f);
}

// ---------------------------------------------------------------------------
// 5. Editing per-band max-power spinbox saves into PaProfileManager.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::editing_max_power_spinbox_saves_into_profile_manager()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* spin = page.maxPowerSpinForTest(Band::Band80m);
    QVERIFY(spin != nullptr);

    spin->setValue(150.0);

    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);

    const PaProfile* active = mgr->activeProfile();
    QVERIFY(active != nullptr);
    QCOMPARE(active->getMaxPower(Band::Band80m), 150.0f);
}

// ---------------------------------------------------------------------------
// 6. Toggling the per-band Use Max checkbox saves into PaProfileManager.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::toggling_use_max_power_checkbox_saves_into_profile_manager()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* check = page.useMaxPowerCheckForTest(Band::Band80m);
    QVERIFY(check != nullptr);

    check->setChecked(true);

    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);

    const PaProfile* active = mgr->activeProfile();
    QVERIFY(active != nullptr);
    QVERIFY(active->getMaxPowerUse(Band::Band80m));
}

// ---------------------------------------------------------------------------
// 7. New button creates a profile via the test-seam-injected name. Combo
//    grows by one, the new profile becomes active.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::new_button_creates_profile_and_sets_active()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* combo = page.profileComboForTest();
    auto* btnNew = page.newButtonForTest();
    QVERIFY(combo != nullptr);
    QVERIFY(btnNew != nullptr);

    const int initialCount = combo->count();

    page.setNextProfileNameForTest(QStringLiteral("TestProfile"));
    btnNew->click();

    QCOMPARE(combo->count(), initialCount + 1);
    QVERIFY(combo->findText(QStringLiteral("TestProfile")) >= 0);

    auto* mgr = model.paProfileManager();
    QCOMPARE(mgr->activeProfileName(), QStringLiteral("TestProfile"));
}

// ---------------------------------------------------------------------------
// 8. Copy button copies the active profile. Pre-edit the active profile to
//    a custom value, then copy via test-seam name "MyCopy"; the new profile
//    must contain the same custom value.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::copy_button_copies_active_profile_with_custom_values()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    // Edit the gain spinbox on the currently-active profile.
    auto* spin = page.gainSpinForTest(Band::Band20m);
    QVERIFY(spin != nullptr);
    spin->setValue(42.5);

    auto* btnCopy = page.copyButtonForTest();
    QVERIFY(btnCopy != nullptr);

    page.setNextProfileNameForTest(QStringLiteral("MyCopy"));
    btnCopy->click();

    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);
    const PaProfile* copied = mgr->profileByName(QStringLiteral("MyCopy"));
    QVERIFY2(copied != nullptr, "Copy must create a new profile by name");
    QCOMPARE(copied->getGainForBand(Band::Band20m, /*driveValue=*/-1), 42.5f);

    // The copy becomes the active profile.
    QCOMPARE(mgr->activeProfileName(), QStringLiteral("MyCopy"));
}

// ---------------------------------------------------------------------------
// 9. Delete button refuses to delete the last remaining profile (Thetis
//    precedent / PaProfileManager last-profile guard).
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::delete_button_refuses_last_remaining_profile()
{
    // Build a fresh model whose PaProfileManager holds only one profile.
    // We bypass the standard 17-entry factory load() and instead hand-seed
    // a single profile, so the page sees exactly one entry on construction.
    AppSettings::instance().clear();

    RadioModel model;
    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);

    mgr->setMacAddress(testMac());
    PaProfile only(QStringLiteral("LastOne"), HPSDRModel::ANAN_G2,
                   /*isFactoryDefault=*/true);
    mgr->saveProfile(QStringLiteral("LastOne"), only);
    mgr->setActiveProfile(QStringLiteral("LastOne"));
    QCOMPARE(mgr->profileNames().size(), 1);

    PaGainByBandPage page(&model);

    auto* btnDelete = page.deleteButtonForTest();
    QVERIFY(btnDelete != nullptr);

    // Bypass QMessageBox::question — confirm the delete intent.
    page.setDeleteConfirmedForTest(true);
    // Suppress the "Cannot delete the last remaining PA profile"
    // QMessageBox::information modal so this headless test does not
    // block waiting for a user OK click.
    page.setSuppressLastProfileWarningForTest(true);
    btnDelete->click();

    // Manager refused; the lone profile remains.
    QCOMPARE(mgr->profileNames().size(), 1);
    QCOMPARE(mgr->activeProfileName(), QStringLiteral("LastOne"));
}

// ---------------------------------------------------------------------------
// 10. Delete button deletes a non-last profile.  Manager has 17 entries
//     after load(); deleting the active one drops us to 16, with a new
//     active set to the lexicographically-first remaining profile.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::delete_button_removes_other_profile()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* combo = page.profileComboForTest();
    auto* btnDelete = page.deleteButtonForTest();
    QVERIFY(combo != nullptr);
    QVERIFY(btnDelete != nullptr);

    // Active profile is "Default - ANAN8000D" after load(ANAN8000D).
    QCOMPARE(combo->currentText(), QStringLiteral("Default - ANAN8000D"));

    page.setDeleteConfirmedForTest(true);
    btnDelete->click();

    auto* mgr = model.paProfileManager();
    QVERIFY(mgr != nullptr);

    // Manager-level manifest is unchanged-by-filter (profileNames returns
    // the full manifest; the page combo uses userVisibleProfileNames for
    // filtering — Issue #202 deep-fix).  Manifest dropped 18 -> 17 after
    // the delete (bumped from 17 -> 18 baseline when commit e82fa68e added
    // ANAN_G2E to factoryProfileNames; Bypass + 16 SKU factory defaults
    // = 17 pre-delete, 16 after Default-ANAN8000D is removed).
    //
    // 2026-05-23 bench tail: ANAN_G2E added a new factory profile, so
    // baseline grew by 1.  Test expectation updated to match.
    QCOMPARE(mgr->profileNames().size(), 17);
    QVERIFY(!mgr->profileNames().contains(QStringLiteral("Default - ANAN8000D")));
    QVERIFY(!mgr->activeProfileName().isEmpty());
    QVERIFY(mgr->activeProfileName() != QStringLiteral("Default - ANAN8000D"));
}

// ---------------------------------------------------------------------------
// 11. Reset Defaults regenerates canonical factory values. Modify the
//     active profile, click Reset Defaults with confirmation, verify the
//     gain values match defaultPaGainsForBand for the model.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::reset_defaults_regenerates_canonical_values()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* spin = page.gainSpinForTest(Band::Band80m);
    QVERIFY(spin != nullptr);

    // Pre-edit to a non-canonical value.
    spin->setValue(10.0);

    auto* btnReset = page.resetButtonForTest();
    QVERIFY(btnReset != nullptr);

    page.setResetConfirmedForTest(true);
    btnReset->click();

    // After reset, the spinbox should reflect the canonical default for
    // ANAN8000D 80m (50.5 dB per Thetis clsHardwareSpecific.cs).
    QCOMPARE(spin->value(), 50.5);
}

// ---------------------------------------------------------------------------
// 12. Auto-calibrate checkbox is present and toggleable. Phase 7 wires the
//     actual sweep flow; Phase 6 only verifies the checkbox renders.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::auto_cal_checkbox_present_and_toggleable()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* check = page.autoCalibrateCheckForTest();
    QVERIFY2(check != nullptr,
             "PaGainByBandPage must own a chkAutoPACalibrate checkbox");

    check->setChecked(true);
    QVERIFY(check->isChecked());
    check->setChecked(false);
    QVERIFY(!check->isChecked());
}

// ---------------------------------------------------------------------------
// 13. Warning label appears when active profile diverges from canonical
//     factory values; hides when reset.
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::warning_label_visible_when_profile_diverges()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    auto* warningLabel = page.warningLabelForTest();
    QVERIFY(warningLabel != nullptr);

    // Default factory profile straight off load() — no divergence yet.
    QVERIFY2(!warningLabel->isVisible() || warningLabel->text().isEmpty()
                 || !warningLabel->isVisibleTo(&page),
             "Warning label should be hidden for canonical factory profile");

    // Mutate a gain value so the active profile no longer matches canonical
    // (ANAN8000D 80m default = 50.5; 45.0 is in-range per #199 spinbox
    // bounds [38.8, 100.0] and still divergent).
    auto* spin = page.gainSpinForTest(Band::Band80m);
    QVERIFY(spin != nullptr);
    spin->setValue(45.0);

    // The warning should now be visible (page recomputes on every spinbox edit).
    QVERIFY2(warningLabel->isVisibleTo(&page),
             "Warning label must be visible after gain divergence");

    // Reset Defaults via the button restores canonical -> warning hides.
    auto* btnReset = page.resetButtonForTest();
    QVERIFY(btnReset != nullptr);
    page.setResetConfirmedForTest(true);
    btnReset->click();

    QVERIFY2(!warningLabel->isVisibleTo(&page),
             "Warning label must be hidden after Reset Defaults");
}

// ---------------------------------------------------------------------------
// 14. Per-band gain spinbox bounds match Thetis (issue #199). Thetis
//     hard-clamps every nud<Band>M / nudVHF<n> spinbox at minimum 38.8 dB
//     and maximum 100.0 dB (setup.designer.cs:48537-48546 [v2.10.3.13] for
//     nudVHF1, with 24 sibling sites configured identically). The
//     pre-#199 NereusSDR port used minimum 0.0, which let users enter
//     values where computeAudioVolume saturates the drive byte at 1.0 with
//     no further effect on RF power output (funsutton field report:
//     ANAN-10E 80m, value 30.8 produced same RF as 38.8).
// ---------------------------------------------------------------------------
void TstPaGainByBandPageEditor::gain_spinbox_bounds_match_thetis_minimum_38_8()
{
    RadioModel model;
    primeModelWithProfiles(model);

    PaGainByBandPage page(&model);

    for (int n = 0; n < PaGainByBandPage::kPaBandCount; ++n) {
        const Band band = static_cast<Band>(n);
        auto* spin = page.gainSpinForTest(band);
        QVERIFY2(spin != nullptr,
                 qPrintable(QStringLiteral("Missing gain spinbox for band %1")
                                .arg(bandLabel(band))));
        QCOMPARE(spin->minimum(), 38.8);
        QCOMPARE(spin->maximum(), 100.0);
    }
}

QTEST_MAIN(TstPaGainByBandPageEditor)
#include "tst_pa_gain_by_band_page_editor.moc"
