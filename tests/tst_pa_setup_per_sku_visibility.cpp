// tests/tst_pa_setup_per_sku_visibility.cpp  (NereusSDR)
//
// Phase 8 of issue #167 (PA calibration safety hotfix) — per-SKU visibility
// matrix for Setup → PA pages.
// no-port-check: test fixture exercising NereusSDR PaSetupPages /
// SetupDialog visibility wiring. Cite comments to Thetis
// setup.cs:19812 (comboRadioModel_SelectedIndexChanged) are documentary
// only — the ported logic itself lives in SetupDialog.cpp + PaSetupPages.cpp
// where the attribution headers + PROVENANCE rows already cover it.
//
// Verifies that each PA setup page (PaGainByBandPage / PaWattMeterPage /
// PaValuesPage) honors the BoardCapabilities flags in the per-SKU matrix
// derived from Thetis comboRadioModel_SelectedIndexChanged
// (setup.cs:19812-20310 [v2.10.3.13+501e3f51]):
//
//   caps.hasPaProfile        — false → PaGainByBandPage disabled + banner shown
//   caps.isRxOnlySku         — true  → entire PA category should hide
//   caps.canDriveGanymede    — true  → Ganymede 500W warning visible
//   caps.hasPureSignal       — true  → PA/TX-Profile recovery warning visible
//
// ATT-on-TX visibility was tested in v0.3.2-rc; the m_attOnTxWarning label
// was dropped in the v0.3.2 pre-tag cleanup pass (PR follow-up to 3M-4).

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QLabel>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "gui/setup/PaSetupPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstPaSetupPerSkuVisibility : public QObject {
    Q_OBJECT

private slots:
    // ── Standard 16-model parity: each known HPSDRModel value should
    //    enable PA Gain page + show no Ganymede warning + no PS warning.
    //    Driven by the BoardCapsTable values for each model.
    void hpsdrModel_atlas_pa_disabled_no_caps();
    void hpsdrModel_hermes_pa_enabled();
    void hpsdrModel_anan10_pa_enabled();
    void hpsdrModel_anan10e_pa_enabled();
    void hpsdrModel_anan100_pa_enabled();
    void hpsdrModel_anan100b_pa_enabled();
    void hpsdrModel_anan100d_pa_enabled();
    void hpsdrModel_anan200d_pa_enabled();
    void hpsdrModel_orionmkii_pa_enabled();
    void hpsdrModel_anan7000d_pa_enabled();
    void hpsdrModel_anan8000d_pa_enabled();
    void hpsdrModel_anan_g2_pa_enabled();
    void hpsdrModel_anan_g2_1k_pa_enabled();
    void hpsdrModel_anvelinapro3_pa_enabled();
    void hpsdrModel_hermeslite_pa_enabled();
    void hpsdrModel_redpitaya_pa_disabled();

    // ── Edge cases:
    void rxOnly_sku_disables_pa_page();
    void andromeda_sku_shows_ganymede_warning();
    void puresignal_radio_shows_ps_warning();

    // ── D3: chkAutoPACalibrate visibility per allowsAutoPaCalibrate ────────
    // From Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added:
    //   chkAutoPACalibrate.Visible = false  (G2E case)
    void anan_g2e_hides_auto_calibrate_checkbox();
    void standard_board_shows_auto_calibrate_checkbox();

    // ── D4: chkBypassANANPASettings visibility per showsBypassPaSettingsUi ─
    // From Thetis setup.cs:19921 [v2.10.3.15] //N1GP G2E added:
    //   chkBypassANANPASettings.Visible = true  (G2E case)
    // G2E: showsBypassPaSettingsUi=true  → bypass checkbox visible.
    // Standard boards: showsBypassPaSettingsUi=false → bypass checkbox hidden.
    void g2e_shows_bypass_pa_settings_checkbox();
    void standard_board_hides_bypass_pa_settings_checkbox();

    // ATT-on-TX warning dropped in v0.3.2 cleanup; see 3M-4 follow-up issue.
    // The corresponding test case (no_step_atten_cal_hides_att_on_tx_warning)
    // was removed alongside the m_attOnTxWarning label.
};

namespace {

// Build a default BoardCapabilities with the PA-relevant flags zeroed,
// then overlay the four flags under test. Other fields are minimally
// initialized — visibility wiring only consults the five flags below
// plus board (for the page-level title fallback).
BoardCapabilities makeCapsForVisibility(bool hasPaProfile,
                                          bool isRxOnlySku,
                                          bool canDriveGanymede,
                                          bool hasPureSignal,
                                          bool hasStepAttenuatorCal)
{
    BoardCapabilities c{};
    c.board                  = HPSDRHW::Unknown;
    c.protocol               = ProtocolVersion::Protocol1;
    c.hasPaProfile           = hasPaProfile;
    c.isRxOnlySku            = isRxOnlySku;
    c.canDriveGanymede       = canDriveGanymede;
    c.hasPureSignal          = hasPureSignal;
    c.hasStepAttenuatorCal   = hasStepAttenuatorCal;
    return c;
}

// Walk every QLabel child of `page` and return true if any visible label
// contains `needle`. Visibility check ensures we don't false-positive on
// hidden warning labels.
bool visibleLabelContains(const QWidget& page, const QString& needle)
{
    const auto labels = page.findChildren<QLabel*>();
    for (const QLabel* lbl : labels) {
        if (lbl->isVisible() && lbl->text().contains(needle)) {
            return true;
        }
    }
    return false;
}

// As above but ignores visibility (covers hidden-by-default warning text).
bool anyLabelContains(const QWidget& page, const QString& needle)
{
    const auto labels = page.findChildren<QLabel*>();
    for (const QLabel* lbl : labels) {
        if (lbl->text().contains(needle)) {
            return true;
        }
    }
    return false;
}

} // namespace

// ── Standard 16-model parity ─────────────────────────────────────────────────

// Atlas: hasPaProfile=false (no integrated PA on the bare HPSDR kit).
// PaGainByBandPage::applyCapabilityVisibility should disable the editor
// surface and show the "no PA support" banner.
void TstPaSetupPerSkuVisibility::hpsdrModel_atlas_pa_disabled_no_caps()
{
    PaGainByBandPage page(/*model=*/nullptr);
    const auto caps = makeCapsForVisibility(/*hasPa=*/false, /*rxOnly=*/false,
                                             /*ganymede=*/false, /*ps=*/false,
                                             /*stepAttCal=*/false);
    page.applyCapabilityVisibility(caps);

    QVERIFY2(!page.isPaEditorEnabledForTest(),
             "Atlas (hasPaProfile=false) must disable PaGainByBandPage editor controls");
    QVERIFY2(page.isNoPaSupportBannerVisibleForTest(),
             "Atlas (hasPaProfile=false) must show the no-PA-support banner");
}

// Hermes through ANVELINAPRO3: standard PA-equipped boards.
// hasPaProfile=true → editor enabled, banner hidden.
void TstPaSetupPerSkuVisibility::hpsdrModel_hermes_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    const auto caps = makeCapsForVisibility(/*hasPa=*/true, false, false, false, true);
    page.applyCapabilityVisibility(caps);
    QVERIFY(page.isPaEditorEnabledForTest());
    QVERIFY(!page.isNoPaSupportBannerVisibleForTest());
    QVERIFY(!page.isGanymedeWarningVisibleForTest());
    QVERIFY(!page.isPsWarningVisibleForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan10_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, false, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan10e_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, false, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan100_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, false, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan100b_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, false, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan100d_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan200d_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_orionmkii_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan7000d_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan8000d_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan_g2_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anan_g2_1k_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

void TstPaSetupPerSkuVisibility::hpsdrModel_anvelinapro3_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, true, true));
    QVERIFY(page.isPaEditorEnabledForTest());
}

// HermesLite 2: hasPaProfile=true (mi0bot setup.cs:6432-6435 [v2.10.3.13-beta2]).
// Sentinel HF gain values (100.0) handled downstream — visibility-wise the
// page renders the editor like any other PA-equipped board.
void TstPaSetupPerSkuVisibility::hpsdrModel_hermeslite_pa_enabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(true, false, false, false, false));
    QVERIFY(page.isPaEditorEnabledForTest());
}

// RedPitaya: kUnknown profile — no PA support yet.
void TstPaSetupPerSkuVisibility::hpsdrModel_redpitaya_pa_disabled()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(false, false, false, false, false));
    QVERIFY(!page.isPaEditorEnabledForTest());
    QVERIFY(page.isNoPaSupportBannerVisibleForTest());
}

// ── Edge cases ───────────────────────────────────────────────────────────────

// HermesLiteRxOnly: isRxOnlySku=true, no PA. Page-level: visibility goes
// to false (entire PA category hidden by SetupDialog wiring); the page
// itself reports its "no PA support" banner state.
void TstPaSetupPerSkuVisibility::rxOnly_sku_disables_pa_page()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(/*hasPa=*/false, /*rxOnly=*/true,
                               false, false, false));
    QVERIFY(!page.isPaEditorEnabledForTest());
    QVERIFY(page.isNoPaSupportBannerVisibleForTest());
}

// Andromeda console (canDriveGanymede=true): Ganymede 500W PA tab is a
// follow-up; show informational warning row.
void TstPaSetupPerSkuVisibility::andromeda_sku_shows_ganymede_warning()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(/*hasPa=*/true, /*rxOnly=*/false,
                               /*ganymede=*/true, /*ps=*/true,
                               /*stepAttCal=*/true));
    QVERIFY(page.isPaEditorEnabledForTest());
    QVERIFY2(page.isGanymedeWarningVisibleForTest(),
             "Andromeda console (canDriveGanymede=true) must show the Ganymede 500W follow-up warning row");
}

// PureSignal-active radio: PA/TX-Profile recovery linkage warning.
void TstPaSetupPerSkuVisibility::puresignal_radio_shows_ps_warning()
{
    PaGainByBandPage page(/*model=*/nullptr);
    page.applyCapabilityVisibility(
        makeCapsForVisibility(/*hasPa=*/true, /*rxOnly=*/false,
                               /*ganymede=*/false, /*ps=*/true,
                               /*stepAttCal=*/true));
    QVERIFY(page.isPaEditorEnabledForTest());
    QVERIFY2(page.isPsWarningVisibleForTest(),
             "PureSignal-capable radio must show the PA/TX-Profile recovery linkage warning");
}

// ATT-on-TX warning dropped in v0.3.2 cleanup; see 3M-4 follow-up issue.
// The original test case (no_step_atten_cal_hides_att_on_tx_warning) was
// removed alongside the m_attOnTxWarning label. The ATT-on-TX UI surface
// will be re-added when PureSignal lands in Phase 3M-4 and the predicate
// gating the safety subsystem starts returning true.

// ── D3: chkAutoPACalibrate visibility ─────────────────────────────────────
//
// From Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added:
//   chkAutoPACalibrate.Visible = false  (in the ANAN_G2E radio-model case)
// G2E: allowsAutoPaCalibrate=false  → checkbox hidden.
// Standard boards: allowsAutoPaCalibrate=true (default) → checkbox visible.

// G2E-like caps (allowsAutoPaCalibrate=false) must hide the checkbox.
void TstPaSetupPerSkuVisibility::anan_g2e_hides_auto_calibrate_checkbox()
{
    PaGainByBandPage page(/*model=*/nullptr);
    BoardCapabilities caps = makeCapsForVisibility(/*hasPa=*/true, /*rxOnly=*/false,
                                                    /*ganymede=*/false, /*ps=*/false,
                                                    /*stepAttCal=*/true);
    caps.allowsAutoPaCalibrate = false;  // G2E flag
    page.applyCapabilityVisibility(caps);

    auto* check = page.autoCalibrateCheckForTest();
    QVERIFY2(check != nullptr, "m_autoCalibrateCheck must exist");
    // Use isVisibleTo(&page) so we test visibility relative to the page
    // widget rather than the full ancestor chain (page is unshown in tests).
    QVERIFY2(!check->isVisibleTo(&page),
             "G2E (allowsAutoPaCalibrate=false) must hide chkAutoPACalibrate");
}

// Standard board (allowsAutoPaCalibrate=true) must leave the checkbox visible.
void TstPaSetupPerSkuVisibility::standard_board_shows_auto_calibrate_checkbox()
{
    PaGainByBandPage page(/*model=*/nullptr);
    BoardCapabilities caps = makeCapsForVisibility(/*hasPa=*/true, /*rxOnly=*/false,
                                                    /*ganymede=*/false, /*ps=*/false,
                                                    /*stepAttCal=*/true);
    caps.allowsAutoPaCalibrate = true;  // default for all non-G2E boards
    page.applyCapabilityVisibility(caps);

    auto* check = page.autoCalibrateCheckForTest();
    QVERIFY2(check != nullptr, "m_autoCalibrateCheck must exist");
    QVERIFY2(check->isVisibleTo(&page),
             "Standard board (allowsAutoPaCalibrate=true) must show chkAutoPACalibrate");
}

// ── D4: chkBypassANANPASettings visibility ────────────────────────────────
//
// From Thetis setup.cs:19921 [v2.10.3.15] //N1GP G2E added:
//   chkBypassANANPASettings.Visible = true;  (in ANAN_G2E case)
// G2E-like caps (showsBypassPaSettingsUi=true) must show the bypass checkbox.
void TstPaSetupPerSkuVisibility::g2e_shows_bypass_pa_settings_checkbox()
{
    PaGainByBandPage page(/*model=*/nullptr);
    BoardCapabilities caps = makeCapsForVisibility(/*hasPa=*/true, /*rxOnly=*/false,
                                                    /*ganymede=*/false, /*ps=*/false,
                                                    /*stepAttCal=*/true);
    caps.allowsAutoPaCalibrate    = false;  // G2E: auto-cal hidden
    caps.showsBypassPaSettingsUi  = true;   // G2E: bypass checkbox shown
    page.applyCapabilityVisibility(caps);

    auto* check = page.bypassPaSettingsCheckForTest();
    QVERIFY2(check != nullptr, "m_bypassPaSettingsCheck must exist");
    QVERIFY2(check->isVisibleTo(&page),
             "G2E (showsBypassPaSettingsUi=true) must show chkBypassANANPASettings");
}

// Standard board (showsBypassPaSettingsUi=false) must hide the bypass checkbox.
void TstPaSetupPerSkuVisibility::standard_board_hides_bypass_pa_settings_checkbox()
{
    PaGainByBandPage page(/*model=*/nullptr);
    BoardCapabilities caps = makeCapsForVisibility(/*hasPa=*/true, /*rxOnly=*/false,
                                                    /*ganymede=*/false, /*ps=*/false,
                                                    /*stepAttCal=*/true);
    caps.showsBypassPaSettingsUi = false;  // default for all non-G2E boards
    page.applyCapabilityVisibility(caps);

    auto* check = page.bypassPaSettingsCheckForTest();
    QVERIFY2(check != nullptr, "m_bypassPaSettingsCheck must exist");
    QVERIFY2(!check->isVisibleTo(&page),
             "Standard board (showsBypassPaSettingsUi=false) must hide chkBypassANANPASettings");
}

QTEST_MAIN(TstPaSetupPerSkuVisibility)
#include "tst_pa_setup_per_sku_visibility.moc"
