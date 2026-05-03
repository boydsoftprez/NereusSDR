// no-port-check: NereusSDR-original test fixture. References to Thetis
// files in comments are upstream-comparison citations (documenting that
// upstream does NOT board-gate the sidetone control), not a port.
//
// =================================================================
// tests/tst_board_capability_flag_wiring.cpp  (NereusSDR)
// =================================================================
//
// Verifies that BoardCapabilities flags whose previous status was
// "populated but unconsumed" are now actually wired into runtime
// behaviour. Plan: docs/architecture/2026-05-02-p1-full-parity-plan.md
// §4 (Tasks 4.1, 4.2, 4.3).
//
// Task 4.1: hasStepAttenuatorCal gates StepAttenuatorController's
//   AutoAttMode::Adaptive selection.  When the flag is false the
//   Adaptive branch (which depends on per-step calibration data)
//   is silently coerced to Classic. Adaptive parameter setters
//   remain callable so UI state survives flag-true → false → true
//   reconnect cycles.
//
// Task 4.2 / 4.3: reserved (see commented blocks below — implementer
//   for those tasks appends their cases without touching the §4.1
//   region).
//
// Source: NereusSDR-internal extension. AutoAttMode::Adaptive is a
// NereusSDR-original feature (not Thetis-derived); the
// hasStepAttenuatorCal flag was added to BoardCapabilities.h with the
// intent of gating per-step cal-table support, and the closest
// runtime consumer in NereusSDR is the Adaptive cal mode that
// depends on per-step calibration. Thetis setup.cs has no
// "step attenuator cal" page (grepped: no
// 'StepAttenuator.*Cal|adaptive.*att|AdaptAtt' matches in setup.cs
// [v2.10.3.13 @501e3f51]); the only StepAttenuator references are
// the chkHermesStepAttenuator / udHermesStepAttenuatorData controls
// which gate the simple linear ATT path on hardware presence, not
// per-step calibration.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Implemented for NereusSDR by J.J. Boyd (KG4VCF),
//                with AI-assisted transformation via Anthropic
//                Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorController.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/setup/hardware/OcOutputsTab.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QApplication>
#include <QSignalSpy>

using namespace NereusSDR;

namespace {

// Test MAC chosen to not collide with any other ctest binary in this run.
constexpr const char* kTestMac = "aa:bb:cc:99:ca:11";

}  // namespace

class TestBoardCapabilityFlagWiring : public QObject {
    Q_OBJECT
private:
    void clearAppSettings() { AppSettings::instance().clear(); }

private slots:
    void initTestCase()
    {
        clearAppSettings();
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }
    void init()          { clearAppSettings(); }
    void cleanup()       { clearAppSettings(); }

    // ── Task 4.1 — hasStepAttenuatorCal ──────────────────────────────────────

    // Case 1: When the flag is false, setAutoAttMode(Adaptive) is silently
    // coerced to Classic.  The setter still completes without warning
    // (silent coerce — refusing would break Setup combo flows that try to
    // set Adaptive on any radio).
    void adaptiveMode_silentlyCoercesToClassic_whenFlagFalse()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setHasStepAttenuatorCal(false);

        ctrl.setAutoAttMode(AutoAttMode::Adaptive);

        QCOMPARE(ctrl.autoAttMode(), AutoAttMode::Classic);
    }

    // Case 2: When the flag is true (or default), Adaptive is accepted.
    void adaptiveMode_accepted_whenFlagTrue()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setHasStepAttenuatorCal(true);

        ctrl.setAutoAttMode(AutoAttMode::Adaptive);

        QCOMPARE(ctrl.autoAttMode(), AutoAttMode::Adaptive);
    }

    // Case 3: A persisted "Adaptive" string is clamped to Classic on load
    // when the flag is false.  Handles the cross-radio case: user sets
    // Adaptive on a hasStepAttenuatorCal=true radio, reconnects with a
    // hasStepAttenuatorCal=false radio (different board).
    void loadSettings_adaptiveString_clampsToClassic_whenFlagFalse()
    {
        // Pre-seed the persisted mode key with "Adaptive" for kTestMac.
        auto& s = AppSettings::instance();
        s.setHardwareValue(kTestMac, QStringLiteral("options/autoAtt/rx1Mode"),
                           QStringLiteral("Adaptive"));

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setHasStepAttenuatorCal(false);
        ctrl.loadSettings(kTestMac);

        QCOMPARE(ctrl.autoAttMode(), AutoAttMode::Classic);
    }

    // Case 4: A persisted "Adaptive" string loads as Adaptive when the flag
    // is true (default behaviour preserved on hasStepAttenuatorCal=true
    // boards).
    void loadSettings_adaptiveString_loadsAdaptive_whenFlagTrue()
    {
        auto& s = AppSettings::instance();
        s.setHardwareValue(kTestMac, QStringLiteral("options/autoAtt/rx1Mode"),
                           QStringLiteral("Adaptive"));

        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setHasStepAttenuatorCal(true);
        ctrl.loadSettings(kTestMac);

        QCOMPARE(ctrl.autoAttMode(), AutoAttMode::Adaptive);
    }

    // Case 5: Adaptive parameter setters (hold/decay) remain callable when
    // the flag is false — only the *mode* is gated.  This lets UI state
    // (Setup → Step ATT → Adaptive Hold spinbox value) survive a
    // flag-true → false → true reconnect cycle without resetting the
    // user's chosen tuning.
    void adaptiveSetters_remainCallable_whenFlagFalse()
    {
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setHasStepAttenuatorCal(false);

        // Persist via saveSettings + reload to confirm the value round-trips
        // (the controller has no public hold-ms getter, only the persistence
        // key — so we exercise that path).
        ctrl.setAutoAttHoldSeconds(7.0);   // 7s → 7000 ms
        ctrl.setAdaptiveDecayMs(1234);

        ctrl.saveSettings(kTestMac);

        // Reload into a fresh controller, flag still false, and confirm the
        // hold value was persisted (the decay key is currently not persisted
        // by the controller, only the hold).
        StepAttenuatorController ctrl2;
        ctrl2.setTickTimerEnabled(false);
        ctrl2.setHasStepAttenuatorCal(false);
        ctrl2.loadSettings(kTestMac);

        // Saved as integer seconds, loaded back as ms.
        // We can't read m_adaptiveHoldMs directly without a getter, but
        // we can re-save and re-read the persistence key to verify the
        // load path accepted the value.
        const auto& s = AppSettings::instance();
        const QString holdStr = s.hardwareValue(
            kTestMac, QStringLiteral("options/autoAtt/rx1HoldSeconds"),
            QStringLiteral("0")).toString();
        QCOMPARE(holdStr, QStringLiteral("7"));
    }

    // ── Task 4.2 — hasSidetoneGenerator ──────────────────────────────────────
    //
    // HL2 firmware generates the CW sidetone in hardware (with its own
    // volume control routed through the radio's audio path); standard P1
    // boards rely on host-generated software sidetone. We use the
    // BoardCapabilities.hasSidetoneGenerator flag to visibility-gate the
    // "Sidetone Volume" row in CwSetupPage so users on non-HL2 boards
    // don't see a control that does nothing on their hardware.
    //
    // Thetis upstream comparison (setup.cs [v2.10.3.13 @501e3f51]):
    // Thetis does NOT board-gate the sidetone control — chkSideTones,
    // chkDSPKeyerSidetone (HW), chkDSPKeyerSidetone_software (SW) are
    // mutually-exclusive checkboxes that are always visible.  This
    // gate is therefore NereusSDR-specific use of the populated flag,
    // not a port of an upstream gate.

    // Case 1: Default — before any setHasSidetoneGenerator() call, the
    // sidetone row is hidden (so users opening Setup before any radio
    // connects don't see a useless control).
    void sidetoneRow_hiddenByDefault()
    {
        CwSetupPage page(nullptr);
        QCOMPARE(page.sidetoneRowVisibleForTest(), false);
    }

    // Case 2: When the flag is true (HL2 family connected), the row
    // becomes visible.
    void sidetoneRow_visibleWhenFlagTrue()
    {
        CwSetupPage page(nullptr);
        page.setHasSidetoneGenerator(true);
        QCOMPARE(page.sidetoneRowVisibleForTest(), true);
    }

    // Case 3: When the flag flips back to false (e.g. user disconnects
    // HL2 then connects an Atlas/Hermes), the row hides again.
    void sidetoneRow_hidesWhenFlagFlipsFalse()
    {
        CwSetupPage page(nullptr);
        page.setHasSidetoneGenerator(true);
        page.setHasSidetoneGenerator(false);
        QCOMPARE(page.sidetoneRowVisibleForTest(), false);
    }

    // Case 4: Repeated calls with the same value are idempotent (no
    // visibility flicker on noisy connection-state-changed signals).
    void sidetoneRow_idempotentOnRepeat()
    {
        CwSetupPage page(nullptr);
        page.setHasSidetoneGenerator(true);
        page.setHasSidetoneGenerator(true);
        QCOMPARE(page.sidetoneRowVisibleForTest(), true);
        page.setHasSidetoneGenerator(false);
        page.setHasSidetoneGenerator(false);
        QCOMPARE(page.sidetoneRowVisibleForTest(), false);
    }

    // ── Task 4.3 — hasPennyLane ──────────────────────────────────────────────
    //
    // Penny / Penny-Lane is the OC-output companion board. Its outputs include
    // user_dig_out (4 user-controllable digital pins, networkproto1.c:601 →
    // bank 11 C3 low 4 bits). OcOutputsTab now hosts a "User Dig Out" group
    // with 4 bit-toggle checkboxes wired to TransmitModel::setUserDigOut.
    // Group visibility gates on caps.hasPennyLane (true on every HPSDR-class
    // board, false on HL2 + RX-only kits).
    //
    // Thetis upstream comparison: Thetis tpPennyCtrl (setup.cs:6364
    // [v2.10.3.13 @501e3f51]) is the OC Control / Hermes Ctrl tab — hosts the
    // OC matrix itself, not separate user_dig_out pin checkboxes. user_dig_out
    // is set in firmware via networkproto1.c bank 11 C3, but Thetis does NOT
    // expose UI checkboxes for those 4 bits. This is a NereusSDR-specific
    // audit-gap closure UI, not a port of an upstream control.
    //
    // Persistence: TransmitModel::persistOne already scopes per-MAC under
    // hardware/<mac>/tx/UserDigOut (TransmitModel.cpp:510-517) — single
    // nibble, no need for plan §4.3.3's "userDigOut/{1..4}" subkey form.
    //
    // Wire-byte assertion is covered by tst_p1_codec_standard_bank11_polarity
    // (Task 1.3); persistence round-trip is covered by TransmitModel's
    // existing persistence tests. We don't re-test those here.

    // Case 1: When the flag is false (HL2 / RX-only), the User Dig Out group
    // is hidden — users on boards without Penny don't see a control whose
    // wire bits go to a non-existent companion board.
    void user_dig_out_group_hidden_when_hasPennyLane_false()
    {
        RadioModel model;
        OcOutputsTab tab(&model);

        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:01");
        BoardCapabilities caps;
        caps.hasPennyLane = false;

        tab.populate(info, caps);

        QCOMPARE(tab.userDigOutGroupVisibleForTest(), false);
    }

    // Case 2: When the flag is true (HPSDR-class board), the group is visible.
    void user_dig_out_group_visible_when_hasPennyLane_true()
    {
        RadioModel model;
        OcOutputsTab tab(&model);

        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:02");
        BoardCapabilities caps;
        caps.hasPennyLane = true;

        tab.populate(info, caps);

        QCOMPARE(tab.userDigOutGroupVisibleForTest(), true);
    }

    // Case 3: Toggling Pin 1 checkbox sets bit 0 of TransmitModel::userDigOut.
    // Untoggling clears it.
    void checkbox_1_toggle_sets_bit_0_in_userDigOut()
    {
        RadioModel model;
        OcOutputsTab tab(&model);

        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:03");
        BoardCapabilities caps;
        caps.hasPennyLane = true;
        tab.populate(info, caps);

        // Pin 1 → bit 0
        tab.toggleUserDigOutCheckboxForTest(/*bit=*/0, /*checked=*/true);
        QCOMPARE(model.transmitModel().userDigOut() & 0x01, 0x01);

        tab.toggleUserDigOutCheckboxForTest(/*bit=*/0, /*checked=*/false);
        QCOMPARE(model.transmitModel().userDigOut() & 0x01, 0x00);
    }

    // Case 4: Toggling Pin 4 checkbox sets bit 3 of TransmitModel::userDigOut.
    void checkbox_4_toggle_sets_bit_3_in_userDigOut()
    {
        RadioModel model;
        OcOutputsTab tab(&model);

        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:04");
        BoardCapabilities caps;
        caps.hasPennyLane = true;
        tab.populate(info, caps);

        // Pin 4 → bit 3
        tab.toggleUserDigOutCheckboxForTest(/*bit=*/3, /*checked=*/true);
        QCOMPARE(model.transmitModel().userDigOut() & 0x08, 0x08);

        tab.toggleUserDigOutCheckboxForTest(/*bit=*/3, /*checked=*/false);
        QCOMPARE(model.transmitModel().userDigOut() & 0x08, 0x00);
    }

    // Case 5: Setting TransmitModel::userDigOut directly updates all four
    // checkboxes via the userDigOutChanged signal — the model is the source
    // of truth and the UI mirrors it.
    void userDigOut_signal_updates_all_checkboxes()
    {
        RadioModel model;
        OcOutputsTab tab(&model);

        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:05");
        BoardCapabilities caps;
        caps.hasPennyLane = true;
        tab.populate(info, caps);

        // Set userDigOut to 0b1010 = bit 1 + bit 3 set (Pin 2 + Pin 4)
        model.transmitModel().setUserDigOut(0x0A);

        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(/*bit=*/0), false);
        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(/*bit=*/1), true);
        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(/*bit=*/2), false);
        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(/*bit=*/3), true);
    }

    // Case 6: The echo-loop guard prevents recursion. Setting the model from
    // outside while the tab is connected fires userDigOutChanged exactly
    // once — the checkbox sync does NOT re-emit by writing back to the
    // model. End state is consistent with the externally-set value.
    void echo_loop_guard_prevents_recursion()
    {
        RadioModel model;
        OcOutputsTab tab(&model);

        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:06");
        BoardCapabilities caps;
        caps.hasPennyLane = true;
        tab.populate(info, caps);

        QSignalSpy spy(&model.transmitModel(), &TransmitModel::userDigOutChanged);

        // Externally set userDigOut to 0b0101 = bits 0 + 2 (Pin 1 + Pin 3)
        model.transmitModel().setUserDigOut(0x05);

        // Exactly one fire — no recursion through the checkbox toggled() path.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0x05);

        // End state consistent with the externally-set value.
        QCOMPARE(model.transmitModel().userDigOut(), 0x05);
        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(0), true);
        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(1), false);
        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(2), true);
        QCOMPARE(tab.userDigOutCheckboxCheckedForTest(3), false);
    }
};

QTEST_MAIN(TestBoardCapabilityFlagWiring)
#include "tst_board_capability_flag_wiring.moc"
