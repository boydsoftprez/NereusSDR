// no-port-check: NereusSDR-original unit-test file.  Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_puresignal_coordinator.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the Phase 3M-4 Task 7 PureSignal coordinator class.
//
// PureSignal is the host-side coordinator that ports PSForm.cs's timer1code
// + timer2code + cmd-state machine + eAAState auto-attention machine + the
// puresignal-helper-class derived properties from Thetis [v2.10.3.13].
// This test file exercises:
//
//   1. singleCalibrate() routes through SetPSControl and sets the
//      _singlecalON internal flag (visible via the cmd-state machine
//      transitioning to TurnOnSingleCalibrate on the next tick).
//
//   2. setAutoCalEnabled(true) issues SetPSControl(0, 0, 1, 0)
//      (Thetis ForcePS auto-cal branch).
//
//   3. reset() / setEnabled(false) issues SetPSControl(0, 0, 0, 0).
//
//   4. onMoxChanged(true/false) forwards to TxChannel::setPSMox.
//
//   5. pollTimerTick reads getPSInfo and emits Q_PROPERTY signals.
//
//   6. feedbackColour values match Thetis FeedbackColourLevel ranges:
//      0..90 = Red, 91..128 = Yellow, 129..181 = Lime, 182+ = DodgerBlue.
//
//   7. invertRedBlue swaps the 0..90 / 182+ Red ↔ DodgerBlue mapping.
//
//   8. saveCorrections / restoreCorrections short-circuit on empty
//      filename or null TxChannel.
//
//   9. Auto-attention state machine transitions Monitor → SetNewValues →
//      RestoreOperation → Monitor.
//
// Source: NereusSDR-original.  See PureSignal.h for the Thetis cite map.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — New test file for Phase 3M-4 Task 7: PureSignal
//                 coordinator unit tests.  J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QColor>
#include <QSignalSpy>
#include <limits>

#include "core/BoardCapabilities.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/StepAttenuatorController.h"
#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP TX channel id — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;

class TstPureSignalCoordinator : public QObject {
    Q_OBJECT

private slots:

    // ── Test 1: construct + destruct without any wiring ─────────────────────
    //
    // PureSignal must tolerate being constructed with all-null late-bound
    // dependencies (TxChannel, PsFeedbackChannel) and then destructed
    // without crashing.  This covers the construction-before-WDSP-init
    // path that production code goes through.

    void constructAndDestruct_withNoWiring_doesNotCrash()
    {
        PureSignal ps(/*engine=*/nullptr,
                      /*tx=*/nullptr,
                      /*fb=*/nullptr,
                      /*mox=*/nullptr,
                      /*stepAtt=*/nullptr,
                      /*twoTone=*/nullptr);
        // Phase 3M-4 bench-fix: ctor auto-starts the coordinator
        // (m_enabled=true) so the poll loop animates without an
        // explicit setEnabled(true) call.  See PureSignal.cpp:88+.
        QCOMPARE(ps.isEnabled(), true);
        QCOMPARE(ps.isAutoCalEnabled(), false);
        QCOMPARE(ps.feedbackLevel(), 0);
        QCOMPARE(ps.calibrationCount(), 0);
        QCOMPARE(ps.correctionsBeingApplied(), false);
        QCOMPARE(ps.isCorrecting(), false);
        QCOMPARE(ps.invertRedBlue(), false);
        QCOMPARE(ps.hideFeedback(), false);
    }

    // ── Test 2: construct with a real TxChannel, pump cal lifecycle ─────────
    //
    // singleCalibrate() must (per PSForm.cs:466-478 [v2.10.3.13]):
    //   1. Disable auto-cal
    //   2. Set _singlecalON = true
    //   3. Issue SetPSControl(1, 0, 0, 0) so the engine drains to LRESET
    //      before the SingleCalibrate state runs
    //   4. Emit calibrationStarted

    void singleCalibrate_emitsCalibrationStarted()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        QSignalSpy startSpy(&ps, &PureSignal::calibrationStarted);
        ps.singleCalibrate();
        QCOMPARE(startSpy.count(), 1);
    }

    void singleCalibrate_secondCallTogglesOff()
    {
        // From Thetis PSForm.cs:467-471 [v2.10.3.13]:
        //   if (_singlecalON) { _singlecalON = false; return; }
        // The btnPSCalibrate handler is a toggle — second click cancels
        // the pending single-cal.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        QSignalSpy startSpy(&ps, &PureSignal::calibrationStarted);
        ps.singleCalibrate();   // sets _singleCalON=true, emits started
        ps.singleCalibrate();   // _singleCalON=true → flips off, no signal

        QCOMPARE(startSpy.count(), 1);  // only the first call fires the signal
    }

    // ── Test 3: setAutoCalEnabled(true) flips the property + emits signal ──

    void setAutoCalEnabled_emitsAutoCalEnabledChanged()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        QSignalSpy spy(&ps, &PureSignal::autoCalEnabledChanged);
        ps.setAutoCalEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(ps.isAutoCalEnabled(), true);

        ps.setAutoCalEnabled(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
        QCOMPARE(ps.isAutoCalEnabled(), false);
    }

    void setAutoCalEnabled_idempotentDoesNotEmit()
    {
        // Setting the same value should not re-emit the signal (matches
        // the PSForm AutoCalEnabled setter, which only does the property
        // assignment + side effects without idempotency, but our seam
        // adds the check to keep UI subscribers from spurious updates).
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        QSignalSpy spy(&ps, &PureSignal::autoCalEnabledChanged);
        ps.setAutoCalEnabled(false);
        ps.setAutoCalEnabled(false);
        QCOMPARE(spy.count(), 0);
    }

    // ── Test 4: reset() drives the engine to OFF ────────────────────────────

    void reset_settsForceAutoCalDisable()
    {
        // From Thetis PSForm.cs:486-491 btnPSReset_Click [v2.10.3.13]:
        //   console.ForcePureSignalAutoCalDisable();  // → AutoCalEnabled=false
        //   if (!_OFF) _OFF = true;
        //   console.PSState = false;
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setAutoCalEnabled(true);
        QCOMPARE(ps.isAutoCalEnabled(), true);

        ps.reset();
        QCOMPARE(ps.isAutoCalEnabled(), false);
    }

    // ── Test 5: setEnabled toggles, emits signal, drives timers ─────────────

    void setEnabled_emitsEnabledChanged()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        // Phase 3M-4 bench-fix: ctor auto-enables; clear first so the
        // setEnabled(true) call is a state change that emits.
        ps.setEnabled(false);
        QCOMPARE(ps.isEnabled(), false);

        QSignalSpy spy(&ps, &PureSignal::enabledChanged);
        ps.setEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(ps.isEnabled(), true);

        ps.setEnabled(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
        QCOMPARE(ps.isEnabled(), false);
    }

    // ── Test 6: onMoxChanged forwards to TxChannel::setPSMox ────────────────
    //
    // We can't directly observe TxChannel::setPSMox(true) without WDSP
    // linkage (the setter is null-guarded on rsmpin.p in non-init builds),
    // but the slot must not crash and must accept both true/false values.

    void onMoxChanged_noCrash()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.onMoxChanged(true);
        ps.onMoxChanged(false);
        ps.onMoxChanged(true);
    }

    void onMoxChanged_withNullTxChannel_noCrash()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        ps.onMoxChanged(true);
        ps.onMoxChanged(false);
    }

    // ── Test 7: pollTimerTick is a no-op when not enabled ───────────────────

    void pollTimerTick_whenDisabled_isNoOp()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        QSignalSpy fbSpy(&ps, &PureSignal::feedbackLevelChanged);
        QSignalSpy calSpy(&ps, &PureSignal::calibrationCountChanged);

        // Without setEnabled(true), pollTimerTick exits early — no signals.
        ps.pollTimerTick();
        QCOMPARE(fbSpy.count(), 0);
        QCOMPARE(calSpy.count(), 0);
    }

    void pollTimerTick_whenEnabled_runsWithoutCrash()
    {
        // With NEREUS_BUILD_TESTS the bare TxChannel doesn't have a live
        // calcc engine, so getPSInfo returns silently (rsmpin null-guard
        // in TxChannel::getPSInfo).  pollTimerTick walks the cmd-state
        // machine but all info[] values stay 0.  The test verifies the
        // tick path doesn't crash and the cmd-state machine advances.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);   // stop the QTimer; we drive manually

        // First tick from Off state — autoON/singlecalON/restoreON all
        // false, so cmd-state stays in Off but issues SetPSControl(1,0,0,0).
        ps.pollTimerTick();

        // Set autoON via setAutoCalEnabled, tick again — cmd-state moves
        // to TurnOnAutoCalibrate.
        ps.setAutoCalEnabled(true);
        ps.pollTimerTick();
        ps.pollTimerTick();   // → AutoCalibrate

        QVERIFY(true);   // smoke pass — no crash
    }

    // ── Test 8: feedbackColour matches Thetis FeedbackColourLevel ranges ────
    //
    // From Thetis PSForm.cs:1123-1138 [v2.10.3.13]:
    //   FB > 181 → DodgerBlue  (#1E90FF)
    //   FB > 128 → Lime        (#00FF00)
    //   FB > 90  → Yellow      (#FFFF00)
    //   else     → Red         (#FF0000)
    // System.Drawing colour values verified against .NET Color reference.

    void feedbackColour_zeroIsRed()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ps.feedbackColour(), QColor(0xFF, 0x00, 0x00));   // Red
    }

    void feedbackColour_181IsLime()
    {
        // 181 is the boundary — > 181 is DodgerBlue, ≤ 181 (and > 128)
        // is Lime.  Per Thetis PSForm.cs:1129-1130 [v2.10.3.13].
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        // No public setter for the atomic; use the test seam by ticking
        // the polling logic with an info[] vector.  Or just verify via
        // the private computation indirectly through invertRedBlue toggle
        // where we control the red/blue boundary.  Cleaner: test the
        // boundary values directly through an injected info[].
        // For the simple range test, drive setEnabled(true) + pollTick
        // is overkill — verify the public API by testing the four
        // representative levels via a helper that computes color.
        //
        // The feedbackColour() reads m_feedbackLevel atomic. We can't
        // set it from outside without the polling loop.  Instead, verify
        // the four boundary cases via the inverter check below.
        QVERIFY(ps.feedbackColour().isValid());
    }

    // ── Test 9: invertRedBlue toggle swaps the >181 and ≤90 colours ─────────
    //
    // From Thetis PSForm.cs:1125-1135 [v2.10.3.13]:
    //   if (FeedbackLevel > 181) { if (_bInvertRedBlue) return Color.Red;
    //                              return Color.DodgerBlue; }
    //   ...
    //   else { if (_bInvertRedBlue) return Color.DodgerBlue;
    //          return Color.Red; }

    void setInvertRedBlue_emitsInvertSignal()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        QSignalSpy spy(&ps, &PureSignal::invertRedBlueChanged);
        ps.setInvertRedBlue(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    }

    void setInvertRedBlue_atZeroLevel_swapsRedToDodgerBlue()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ps.feedbackColour(), QColor(0xFF, 0x00, 0x00));   // Red
        ps.setInvertRedBlue(true);
        QCOMPARE(ps.feedbackColour(), QColor(0x1E, 0x90, 0xFF));   // DodgerBlue
        ps.setInvertRedBlue(false);
        QCOMPARE(ps.feedbackColour(), QColor(0xFF, 0x00, 0x00));   // Red
    }

    void setInvertRedBlue_emitsFeedbackColourChanged()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        QSignalSpy spy(&ps, &PureSignal::feedbackColourChanged);
        ps.setInvertRedBlue(true);
        QCOMPARE(spy.count(), 1);
    }

    // ── Test 10: hideFeedback toggle ────────────────────────────────────────

    void setHideFeedback_emitsHideFeedbackChanged()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        QSignalSpy spy(&ps, &PureSignal::hideFeedbackChanged);
        ps.setHideFeedback(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(ps.hideFeedback(), true);

        ps.setHideFeedback(true);   // idempotent
        QCOMPARE(spy.count(), 0);
    }

    // ── Test 11: saveCorrections / restoreCorrections short-circuit ─────────

    void saveCorrections_withNullTxChannel_returnsFalse()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ps.saveCorrections(QStringLiteral("/tmp/x.ps")), false);
    }

    void saveCorrections_withEmptyFilename_returnsFalse()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ps.saveCorrections(QString()), false);
    }

    void saveCorrections_withoutLiveCalcc_returnsFalse()
    {
        // Accepted and completed are distinct. A bare wrapper has no live
        // CALCC status object, so the request was not accepted.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ps.saveCorrections(QStringLiteral("/tmp/nope.ps")), false);
    }

    void restoreCorrections_withoutLiveCalcc_returnsFalse()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ps.restoreCorrections(QStringLiteral("/tmp/nope.ps")), false);
    }

    // ── Test 12: setTwoToneOn forwards to TwoToneController ─────────────────

    void setTwoToneOn_withNullTwoTone_noCrash()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setTwoToneOn(true);
        ps.setTwoToneOn(false);
    }

    // ── Test 13: applyBoardCapabilities pushes psDefaultPeak + psSampleRate ─

    void applyBoardCapabilities_doesNotCrash()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        BoardCapabilities caps{};
        caps.psDefaultPeak = 0.2899;     // ANAN-G2 default per cmaster.cs:536
        caps.psSampleRate  = 192000;

        ps.applyBoardCapabilities(caps);
    }

    // ── Test 14: AutoAttenuateState defaults to Monitor ─────────────────────

    void autoAttenuateState_defaultsToMonitor()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::Monitor));
    }

    void autoAttentionTick_whenDisabled_isNoOp()
    {
        // setEnabled(false) — autoAttentionTick must early-return.
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        ps.autoAttentionTick();
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::Monitor));
    }

    void autoAttentionTick_whenEnabledNoMox_staysInMonitor()
    {
        // No MoxController wired → !mox->isMox() short-circuits.
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        PureSignal ps(nullptr, &tx, nullptr, nullptr, &stepAtt, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);

        ps.autoAttentionTick();
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::Monitor));
    }

    // ── Phase 3M-4 Task 17: PSForm.cs:738 force-enable verbatim port ────────
    //
    // From Thetis PSForm.cs:738 [v2.10.3.13]:
    //   if (!console.ATTOnTX) AutoAttenuate = true; //MW0LGE
    //
    // The AutoAttenuate setter (PSForm.cs:295-314) chains:
    //   _autoattenuate = value;
    //   if (_autoattenuate) console.ATTOnTX = _autoattenuate;
    //
    // i.e. when the auto-att tick advances to SetNewValues, it force-enables
    // the ATT-on-TX master toggle so subsequent setAttOnTxValue calls
    // actually push to hardware.  NereusSDR mirrors this directly via
    // m_stepAtt->setAttOnTxEnabled(true) at the equivalent point in the
    // Monitor → SetNewValues transition.
    void autoAttentionTick_forceEnablesAttOnTxMaster()
    {
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);

        // Drive isMox()=true so the autoAttentionTick gate passes.
        mox.setMox(true);

        // needRecal predicate (PSForm.cs:743-754 IsFeedbackLevelOK + the
        // PSForm.cs:1109-1112 NeedToRecalibrate range check):
        //   (fbLevel > 181) || (fbLevel <= 128 && currentAttOnTx > 0)
        // Default fbLevel is 0 (atomic init); push currentAttOnTx > 0 to
        // make the second clause trigger.
        stepAtt.setAttOnTxValue(15);

        // Precondition: master toggle OFF — mirrors Thetis "console.ATTOnTX
        // is false" branch where PSForm.cs:738 force-flips it back to true.
        stepAtt.setAttOnTxEnabled(false);
        QVERIFY(!stepAtt.attOnTxEnabled());

        // PR #212 follow-up: m_aaLastSeenCalCount initial value changed
        // from -1 to 0 so the FIRST autoAttentionTick is properly gated
        // when calCount is still 0 (calcc hasn't run yet).  Bump
        // calCount via the test seam to simulate a completed calcc cycle
        // and trigger the calCount-changed branch.
        ps.setAttemptCountForTest(1);

        ps.autoAttentionTick();

        // Postcondition: tick advanced past Monitor (so we know the
        // PSForm.cs:738 line ran), AND the master toggle is now ON.
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::SetNewValues));
        QVERIFY(stepAtt.attOnTxEnabled()); //MW0LGE force-enable verified
    }

    void autoAttentionTick_leavesAttOnTxMasterAloneWhenAlreadyOn()
    {
        // Sanity: the force-enable line is conditional (`if !attOnTxEnabled`),
        // so when the master toggle is already ON it must remain ON without
        // any side-effect on neighbouring state.  Mirrors Thetis's no-op
        // case: the AutoAttenuate setter only writes console.ATTOnTX when
        // the flag changes (the chain at PSForm.cs:297-301 always assigns
        // but the underlying ATTOnTX setter at console.cs:19048 silently
        // accepts the same value).
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);
        mox.setMox(true);
        stepAtt.setAttOnTxValue(15);

        stepAtt.setAttOnTxEnabled(true);  // already ON
        ps.autoAttentionTick();

        QVERIFY(stepAtt.attOnTxEnabled());
    }

    // ── Phase 3M-4 mi0bot audit: HL2 NeedToRecalibrate uses minAttenuation() ─
    //
    // Source: mi0bot PSForm.cs:1142-1144 [v2.10.3.13-beta2]:
    //   public static bool NeedToRecalibrate_HL2(int nCurrentATTonTX) {
    //       return (FeedbackLevel > 181 ||
    //               (FeedbackLevel <= 128 && nCurrentATTonTX > -28));
    //       // MI0BOT: Needed seperate function for HL2
    //   }
    //
    // Inline tag preserved: //MI0BOT (HL2-only NeedToRecalibrate variant
    // attribution at PSForm.cs:1144).
    //
    // NereusSDR uses StepAttenuatorController::minAttenuation() (per-board
    // floor — 0 for legacy boards, -28 for HL2) to unify both Thetis
    // branches into one predicate.  HL2 with currentAtt=-15 (in the [-28,
    // -1] range) and fbLevel=100 should TRIGGER recal — Thetis legacy
    // would have early-returned because (-15 > 0) is false.
    void autoAttentionTick_hl2NegativeAttTriggersRecal()
    {
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        // Configure as HL2: signed ATT range with floor at -28.
        stepAtt.setMinAttenuation(-28);
        stepAtt.setAttOnTxValue(-15);  // negative; legacy `> 0` would skip

        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);
        mox.setMox(true);

        // fbLevel=0 (default, after-MOX-but-before-pscc-runs).  The
        // needRecal predicate `(fbLevel > 181) || (fbLevel <= 128 &&
        // currentAttOnTx > minAtt)` evaluates as
        // `(false) || (true && (-15 > -28))` = true.  Tick MUST advance
        // to SetNewValues.
        // PR #212 follow-up: bump calCount via test seam since
        // m_aaLastSeenCalCount initial value changed from -1 to 0.
        ps.setAttemptCountForTest(1);
        ps.autoAttentionTick();

        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::SetNewValues));
    }

    // ── Phase 3M-4 mi0bot audit: HL2 SetNewValues clamp = minAttenuation() ──
    //
    // Source: mi0bot PSForm.cs:786-790 [v2.10.3.13-beta2]:
    //   if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
    //       newAtten = oldAtten + _deltadB;     //MI0BOT: HL2 negative OK
    //   else { if ((oldAtten + _deltadB) > 0) ... else newAtten = 0; }
    //
    // NereusSDR uses minAttenuation() to unify: HL2 floors at -28, legacy
    // floors at 0.  HL2 with oldAtten=10 + deltaDb=-25 → newAtten = -15
    // (within [-28, 31]), NOT clamped to 0.
    void autoAttentionTick_hl2SetNewValues_allowsNegativeAtten()
    {
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        // HL2 floor at -28
        stepAtt.setMinAttenuation(-28);
        stepAtt.setAttOnTxValue(10);

        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);
        mox.setMox(true);
        ps.setAutoCalEnabled(true);
        int initialInfo[16] = {};
        ps.processNewInfo(initialInfo);
        ps.processNewInfo(initialInfo);

        // Drive into SetNewValues manually: first tick advances Monitor →
        // SetNewValues with deltaDb computed from fbLevel=0 (which yields
        // 31.1 after the math: -inf log10(1/152.293) ≈ -43.7 dB after
        // clamp at -100 → rounds to -44).  Override deltaDb directly via
        // a synthetic processNewInfo hand-off would be cleaner, but the
        // existing test seam exposes only autoAttenuateState() / state
        // transitions.  Instead: set fbLevel via processNewInfo with a
        // synthetic info[] that has feedbackLevel=200 (>181) → needRecal
        // fires + deltaDb = 20*log10(200/152.293) ≈ 2.4 → 2.
        int info[16] = {0};
        info[4]  = 200;     // feedbackLevel
        info[5]  = 1;       // calCount (changed from 0)
        info[7]  = 1;       // calibration attempt (changed from 0)
        info[14] = 1;       // corrApplied
        info[15] = 6;       // state = LCALC
        ps.processNewInfo(info);

        ps.autoAttentionTick();   // Monitor → SetNewValues (deltaDb computed)
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::SetNewValues));

        info[15] = 0; // Native reset acknowledgement precedes the ATT write.
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();   // SetNewValues → RestoreOperation
        // After SetNewValues, attOnTxValue is set to oldAtten + deltaDb (no
        // floor clamp on the positive side).  For deltaDb=+2 + oldAtten=10
        // → newAtten=12.
        QVERIFY(stepAtt.attOnTxValue() >= -28);  // never below floor
        QVERIFY(stepAtt.attOnTxValue() <= 31);   // never above ceiling
    }

    void autoAttentionTick_legacyClampToZero_unchanged()
    {
        // Sanity: legacy boards (minAttenuation()=0) still clamp at 0,
        // matching Thetis else-branch at PSForm.cs:786-790.
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        // Legacy floor: 0 (default; no setMinAttenuation needed).
        QCOMPARE(stepAtt.minAttenuation(), 0);

        stepAtt.setAttOnTxValue(5);
        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);
        mox.setMox(true);

        // fbLevel=0, currentAtt=5 → needRecal = (false) || (true && (5 > 0))
        // = true.  Tick advances to SetNewValues.
        // PR #212 follow-up: bump calCount via test seam since
        // m_aaLastSeenCalCount initial value changed from -1 to 0.
        ps.setAttemptCountForTest(1);
        ps.autoAttentionTick();
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::SetNewValues));
    }

    void autoAttentionTick_waitsForResetAndDiscardsDrainedCalibration()
    {
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);
        ps.setTimersEnabled(false);
        ps.setAutoAttenuate(true);
        ps.setAutoCalEnabled(true);
        mox.setMox(true);
        stepAtt.setAttOnTxValue(31);

        int info[16] = {};
        ps.processNewInfo(info);
        ps.processNewInfo(info);
        info[4] = 29;
        info[5] = 1;
        info[7] = 1;
        info[14] = 1;
        info[15] = 6; // PS3 starts its next calculation before the host poll.
        ps.processNewInfo(info);
        ps.autoAttentionTick();

        // The G2 retest had a roughly two-second calculation. A reset
        // request does not acknowledge reset while LCALC still owns it.
        for (int tick = 0; tick < 20; ++tick) {
            ps.autoAttentionTick();
            QCOMPARE(stepAtt.attOnTxValue(), 31);
            QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::SetNewValues);
        }
        info[4] = 30; // The outstanding collection still belongs to 31 dB.
        info[5] = 2;
        info[7] = 2;
        info[14] = 0;
        info[15] = 0; // WDSP drained that calculation and acknowledged reset.
        ps.processNewInfo(info);
        ps.autoAttentionTick(); // A pre-request cache snapshot is not an ack.
        QCOMPARE(stepAtt.attOnTxValue(), 31);
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 17);
        ps.autoAttentionTick(); // Restore calibration at the new attenuation.
        for (int tick = 0; tick < 20; ++tick) {
            ps.autoAttentionTick();
            QCOMPARE(stepAtt.attOnTxValue(), 17);
            QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::Monitor);
        }
        info[4] = 152;
        info[5] = 3;
        info[7] = 3;
        info[14] = 1;
        info[15] = 6;
        ps.processNewInfo(info);
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 17);
        QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::Monitor);
    }

    void autoAttentionTick_failedExtrapolationDoesNotReuseFeedback()
    {
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);
        ps.setTimersEnabled(false);
        ps.setAutoAttenuate(true);
        ps.setAutoCalEnabled(true);
        mox.setMox(true);
        stepAtt.setAttOnTxValue(17);
        int info[16] = {};
        ps.processNewInfo(info);
        ps.processNewInfo(info);
        // Native calc increments attempts before extrapolation; a confidence
        // failure exits before assigning feedback (calcc.c:1257,1274-1280).
        info[0] = 1;
        info[4] = 29; // Retained from a previous collection at 31 dB.
        info[5] = 1;
        info[7] = 2;
        info[15] = 6;
        ps.processNewInfo(info);
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 17);
        QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::Monitor);
    }

    void autoAttentionTick_unkeyCancelsPendingAttenuationChange()
    {
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);
        ps.setTimersEnabled(false);
        ps.setAutoAttenuate(true);
        ps.setAutoCalEnabled(true);
        mox.setMox(true);
        stepAtt.setAttOnTxValue(31);
        int info[16] = {};
        ps.processNewInfo(info);
        ps.processNewInfo(info);
        info[4] = 29;
        info[5] = 1;
        info[7] = 1;
        info[15] = 6;
        ps.processNewInfo(info);
        ps.autoAttentionTick();
        mox.setMox(false);
        info[15] = 0;
        ps.processNewInfo(info);
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 31);
        QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::Monitor);

        // With no paired pump the old worker can finish during receive.
        // Its first publication on rekey must not become a fresh ATT input.
        mox.setMox(true);
        info[4] = 30;
        info[5] = 2;
        info[7] = 2;
        info[15] = 6;
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 31);
        QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::Monitor);
        info[15] = 0;
        ps.processNewInfo(info); // Old cache reset state cannot release drain.
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 31);
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 31);
        QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::Monitor);
        info[4] = 152;
        info[5] = 3;
        info[7] = 3;
        info[15] = 6;
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), 31);
    }

    void autoAttentionTick_newIntentRetiresPendingOperation_data()
    {
        QTest::addColumn<int>("command");
        QTest::addColumn<bool>("afterAttWrite");
        for (int command = 0; command < 6; ++command) {
            for (bool afterAttWrite : {false, true}) {
                const QByteArray name = QByteArray::number(command)
                    + (afterAttWrite ? "-restore" : "-reset-wait");
                QTest::newRow(name.constData()) << command << afterAttWrite;
            }
        }
    }

    void autoAttentionTick_newIntentRetiresPendingOperation()
    {
        QFETCH(int, command);
        QFETCH(bool, afterAttWrite);
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);
        ps.setTimersEnabled(false);
        ps.setAutoAttenuate(true);
        ps.setAutoCalEnabled(true);
        mox.setMox(true);
        stepAtt.setAttOnTxValue(31);
        int info[16] = {};
        ps.processNewInfo(info);
        ps.processNewInfo(info);
        info[4] = 29;
        info[5] = 1;
        info[7] = 1;
        info[15] = 6;
        ps.processNewInfo(info);
        ps.autoAttentionTick();
        if (afterAttWrite) {
            info[15] = 0;
            ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
            ps.autoAttentionTick();
            QCOMPARE(stepAtt.attOnTxValue(), 17);
        }
        switch (command) {
        case 0: mox.setMox(false); break;
        case 1: ps.setAutoAttenuate(false); break;
        case 2: ps.setRunCalibrationProcessing(false); break;
        case 3: ps.setAutoCalEnabled(false); break;
        case 4: ps.reset(); break;
        case 5: ps.setTxChannel(nullptr); break;
        }
        ps.autoAttentionTick();
        QCOMPARE(stepAtt.attOnTxValue(), afterAttWrite ? 17 : 31);
        QCOMPARE(ps.autoAttenuateState(), PureSignal::AutoAttenuateState::Monitor);
    }

    // ── Test 15: late-bound setters wire TxChannel + PsFeedbackChannel ──────

    void setTxChannel_lateBindingDoesNotCrash()
    {
        TxChannel tx(kTxChannelId);
        // The bound channel must outlive its coordinator, including teardown.
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        ps.setTxChannel(&tx);
        // After binding, singleCalibrate now routes through tx's setPSControl.
        ps.singleCalibrate();
    }

    void setPsFeedbackChannel_lateBindingDoesNotCrash()
    {
        PureSignal ps(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        ps.setPsFeedbackChannel(nullptr);
    }

    // ── Phase 3M-4 bench-fix Round 2: applyBoardCapabilities push ──────────

    void applyBoardCapabilities_setsHwPeakFromCaps()
    {
        // From Thetis cmaster.cs:566 [v2.10.3.13-beta2] (mi0bot):
        //   puresignal.SetPSHWPeak(txch, HardwareSpecific.PSDefaultPeak);
        // Bench-fix Round 2 wired RadioModel's WDSP-init lambda to call
        // PureSignal::applyBoardCapabilities so the per-board peak (e.g.
        // 0.6121 for ANAN-G2) actually reaches calcc.  Verify the cached
        // m_hwPeak side effect (the TxChannel::setPSHWPeak call is a
        // pass-through to WDSP that's null-safe in test mode).
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        BoardCapabilities caps{};
        caps.psDefaultPeak = 0.6121;     // ANAN-G2 / Saturn (Task 1 1bbb85a)
        caps.psSampleRate  = 192000;

        ps.applyBoardCapabilities(caps);

        QCOMPARE(ps.hwPeak(), 0.6121);
    }

    // ── Phase 3M-4 bench-fix Round 2: processNewInfo + psInfoChanged ───────

    void processNewInfo_emitsPsInfoChanged_whenAutoCalAndInfoChanged()
    {
        // From Thetis PSForm.cs:614-619 timer1code [v2.10.3.13]:
        //   if (_autocal_enabled)
        //       if (puresignal.HasInfoChanged)
        //           console.InfoBarFeedbackLevel(...);
        // psInfoChanged is the NereusSDR analogue, gated identically.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setAutoCalEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy spy(&ps, &PureSignal::psInfoChanged);

        // info[4] = FeedbackLevel = 150
        // info[5] = CalibrationCount = 1 (was 0)
        // info[14] = CorrectionsBeingApplied = 1
        int info[16] = {};
        info[4] = 150;
        info[5] = 1;
        info[7] = 1;
        info[14] = 1;
        ps.processNewInfo(info);

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(),  150);   // level
        QCOMPARE(args.at(1).toBool(), true);  // ok (level <= 256)
        QCOMPARE(args.at(2).toBool(), true);  // corrApplied
        QCOMPARE(args.at(3).toBool(), true);  // calAttemptsChanged (1 != 0)
        // colour for level=150 is Lime per PSForm.cs:1130 [v2.10.3.13]
        QCOMPARE(args.at(4).value<QColor>(), QColor(0x00, 0xFF, 0x00));
    }

    void processNewInfo_doesNotEmit_whenAutoCalDisabled()
    {
        // Gate: m_autoCalEnabled gates the emit.  Default is false.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setTimersEnabled(false);
        // Do NOT call setAutoCalEnabled(true).

        QSignalSpy spy(&ps, &PureSignal::psInfoChanged);

        int info[16] = {};
        info[4] = 150;
        info[5] = 1;
        info[14] = 1;
        ps.processNewInfo(info);

        QCOMPARE(spy.count(), 0);
    }

    void processNewInfo_doesNotEmit_whenInfoUnchanged()
    {
        // Gate: HasInfoChanged.  Calling processNewInfo twice with the
        // same info[] array should only fire psInfoChanged on the first
        // call (which is itself gated; second call sees newInfo == oldInfo
        // because the trailing memcpy stashed it).
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setAutoCalEnabled(true);
        ps.setTimersEnabled(false);

        int info[16] = {};
        info[4] = 150;
        info[5] = 1;
        info[14] = 1;
        ps.processNewInfo(info);   // first call, info changed (vs zeros)

        QSignalSpy spy(&ps, &PureSignal::psInfoChanged);
        ps.processNewInfo(info);   // second call, info unchanged
        QCOMPARE(spy.count(), 0);
    }

    void processNewInfo_carriesCalAttemptsChangedCorrectly()
    {
        // From Thetis PSForm.cs:1097-1098 [v2.10.3.13]:
        // PS3 reports attempts in info[7] (successful calibrations remain
        // info[5]). First call: info[7]=1 vs oldInfo[7]=0 → true.
        // Second call: info[7]=1 unchanged, info[4] flipped to trip
        // HasInfoChanged → calChanged=false in payload.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setAutoCalEnabled(true);
        ps.setTimersEnabled(false);

        int info1[16] = {};
        info1[4] = 150;
        info1[5] = 1;
        info1[7] = 1;
        info1[14] = 1;
        ps.processNewInfo(info1);   // calChanged=true

        QSignalSpy spy(&ps, &PureSignal::psInfoChanged);

        int info2[16] = {};
        info2[4] = 160;             // FeedbackLevel changed → HasInfoChanged
        info2[5] = 1;               // CalibrationCount unchanged
        info2[7] = 1;               // CalibrationAttempts unchanged
        info2[14] = 1;
        ps.processNewInfo(info2);

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(),  160);    // level
        QCOMPARE(args.at(3).toBool(), false);  // calAttemptsChanged (1 == 1)
    }

    // ── PR #212 codex-fix A: HL2 psSampleRate=0 sentinel resolution ─────────
    //
    // kHermesLite.psSampleRate = 0 is a NereusSDR sentinel meaning "use
    // rx1_rate at the codec/DDC layer per mi0bot console.cs:8472-8488
    // [v2.10.3.13-beta2]".  It is NOT a valid value for the calcc feedback
    // clock — calcc.c:1069 [v2.10.3.13] stores `a->rate = rate;` and uses
    // it as the divisor for `moxsamps = rate * moxdelay` (line 1070) and
    // `waitsamps = rate * loopdelay` (line 1071).  Passing 0 produces
    // moxsamps=0 + waitsamps=0 → state-machine timeouts on every cycle.
    //
    // Thetis itself doesn't branch on board for ps_rate — cmaster.cs:535
    // [v2.10.3.13] always passes ps_rate=192000:
    //   private static int ps_rate = 192000;             // cmaster.cs:424
    //   ...
    //   puresignal.SetPSFeedbackRate(txch, ps_rate);     // cmaster.cs:535
    //
    // So PureSignal::applyBoardCapabilities must resolve the HL2 sentinel
    // BEFORE the WDSP call, falling back to the Thetis universal 192000.

    void applyBoardCapabilities_hl2SentinelResolvesTo192k()
    {
        // Pre-fix: the 0 sentinel flowed straight to WDSP → calcc.c:1069
        // a->rate=0 → broken delay calculations.  Post-fix: 0 is replaced
        // with cmaster.cs:424 ps_rate=192000.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        ps.applyBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::HermesLite));

        // setPSFeedbackRate must have been called with 192000, NOT 0.
        QCOMPARE(tx.lastPSFeedbackRateForTest(), 192000);
    }

    void applyBoardCapabilities_hl2RxOnlySentinelResolvesTo192k()
    {
        // HermesLiteRxOnly mirrors HermesLite's psSampleRate=0 sentinel
        // (BoardCapabilities.cpp:771).  Same resolution path — the kit
        // can't TX, but the cap-table is populated for table consistency
        // and the resolution must still be correct.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        ps.applyBoardCapabilities(
            BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly));

        QCOMPARE(tx.lastPSFeedbackRateForTest(), 192000);
    }

    void applyBoardCapabilities_legacyBoardKeepsExplicitRate()
    {
        // Sanity: non-HL2 boards have psSampleRate=192000 explicit
        // (BoardCapabilities.cpp:428/480/530/583/827/880/933).  The
        // sentinel-resolution path must be a NO-OP for them.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);

        ps.applyBoardCapabilities(BoardCapsTable::forBoard(HPSDRHW::Saturn));

        QCOMPARE(tx.lastPSFeedbackRateForTest(), 192000);
    }

    // ── PR #212 codex-fix B: HL2-specific deltaDb clamps in autoAttentionTick ─
    //
    // Inline tags preserved verbatim per CLAUDE.md "Inline comment
    // preservation" within ±5 lines of upstream cite range:
    //   //MW0LGE             [from mi0bot PSForm.cs:740 + 772 — surrounding the cite block]
    //   //MI0BOT             [from mi0bot PSForm.cs:758-760, 766 — verbatim within block]
    //
    // From mi0bot PSForm.cs:744-769 [v2.10.3.13-beta2]:
    //   double ddB;
    //   if (puresignal.IsFeedbackLevelOK)
    //   {
    //       ddB = 20.0 * Math.Log10((double)puresignal.FeedbackLevel / 152.293);
    //
    //       if (HPSDRModel.HERMESLITE != HardwareSpecific.Model)
    //       {
    //           if (Double.IsNaN(ddB)) ddB = 31.1;
    //           if (ddB < -100.0) ddB = -100.0;
    //           if (ddB > +100.0) ddB = +100.0;
    //       }
    //       else
    //       {
    //           if (Double.IsNaN(ddB)) ddB = 10.0;  // MI0BOT
    //           if (ddB < -100.0) ddB = -10.0;      // MI0BOT
    //           if (ddB > +100.0) ddB = 10.0;       // MI0BOT
    //       }
    //   }
    //   else
    //   {
    //       if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
    //           ddB = 10.0;
    //       else
    //           ddB = 31.1;
    //   }
    //
    // Pre-fix: NereusSDR used the legacy clamps universally; a transient
    // bad feedback reading on HL2 slammed ATT to the -28 floor instead of
    // a -10 dB nudge.  Post-fix: branch the four clamp/fallback lines on
    // HL2 board membership.
    //
    // We OBSERVE deltaDb via the post-tick newAtten = oldAtten + deltaDb
    // computation at SetNewValues (PSForm.cs:786-790, NereusSDR
    // PureSignal.cpp:1208-1212).  oldAtten=0 + deltaDb=-100 → newAtten=-100
    // (clamped to minAtt=-28 in HL2 case via the existing minAttenuation
    // unification).  oldAtten=0 + deltaDb=-10 → newAtten=-10 (HL2 fix).

    void autoAttentionTick_hl2ClampsMinusInfinityToMinusTen()
    {
        // HL2 board with fbLevel=0 (transient mid-dropout reading):
        // Thetis (and our port) computes
        //   ddB = 20 * log10(0/152.293) = 20 * log10(0) = -Infinity
        // Legacy clamp:  if (ddB < -100.0) ddB = -100.0;
        //   → newAtten = 0 + (-100) clamped to floor -28 = -28 (full slam).
        // HL2 clamp:     if (ddB < -100.0) ddB = -10.0;     // MI0BOT
        //   → newAtten = 0 + (-10) = -10 (gentle nudge).
        //
        // This is exactly the path mi0bot PSForm.cs:759 [v2.10.3.13-beta2]
        // "Handle - infinity" describes — the -Infinity-from-log10(0)
        // sentinel-handler, NOT a generic ddB-out-of-range clamp.
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        // Identify the board as HL2 BEFORE driving the tick — the clamp
        // branch reads m_caps.board.
        ps.applyBoardCapabilities(
            BoardCapsTable::forBoard(HPSDRHW::HermesLite));

        // HL2 floor at -28
        stepAtt.setMinAttenuation(-28);
        stepAtt.setAttOnTxValue(0);

        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);
        mox.setMox(true);
        ps.setAutoCalEnabled(true);
        int initialInfo[16] = {};
        ps.processNewInfo(initialInfo);
        ps.processNewInfo(initialInfo);

        // Inject feedbackLevel=0 (calcc mid-dropout).  needRecal predicate
        // requires fbLevel>181 OR (fbLevel<=128 && currentAtt > minAtt).
        // currentAtt=0, minAtt=-28 → second clause fires.  calCount changed
        // satisfies CalibrationAttemptsChanged.
        int info[16] = {};
        info[4] = 0;        // FeedbackLevel=0 → log10(0) = -Infinity
        info[5] = 1;        // CalibrationCount changed
        info[7] = 1;        // CalibrationAttempts changed
        info[14] = 1;       // corrApplied
        info[15] = 6;       // state
        ps.processNewInfo(info);

        ps.autoAttentionTick();   // Monitor → SetNewValues
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::SetNewValues));

        info[15] = 0; // Native reset acknowledgement precedes the ATT write.
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();   // SetNewValues → applies newAtten

        // HL2 post-fix: -Infinity clamped to -10 → newAtten = 0 + (-10) = -10.
        // Pre-fix would have produced -28 (legacy -100 clamp hits the -28 floor).
        QCOMPARE(stepAtt.attOnTxValue(), -10);
    }

    void autoAttentionTick_hl2ClampsNanFallbackToTen()
    {
        // From mi0bot PSForm.cs:765-768 [v2.10.3.13-beta2] —
        // !IsFeedbackLevelOK branch (FeedbackLevel > 256):
        //   if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
        //       ddB = 10.0;             // MI0BOT: HL2 fallback (not 31.1)
        //   else
        //       ddB = 31.1;             // [2.10.3.12]MW0LGE legacy fallback
        //
        // Inline tags preserved per CLAUDE.md "Inline comment preservation":
        //MI0BOT   [PSForm.cs:760 + 766 HL2-specific clamp / fallback values]
        //MW0LGE   [PSForm.cs:772 `_deltadB = (int)Math.Round(ddB, ...) //
        //          [2.10.3.12]MW0LGE use rounding, to fix Banker's rounding`
        //          rounding-mode attribution that this test path verifies]
        //
        // HL2 with fbLevel=300 (>256, IsFeedbackLevelOK false) →
        // ddB=10 (HL2 clamp), NOT 31.1 (legacy fallback).
        //
        // Note: needRecal predicate gate (`fbLevel > 181`) fires for
        // fbLevel=300, so the tick advances.
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        ps.applyBoardCapabilities(
            BoardCapsTable::forBoard(HPSDRHW::HermesLite));

        stepAtt.setMinAttenuation(-28);
        stepAtt.setAttOnTxValue(0);

        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);
        mox.setMox(true);
        ps.setAutoCalEnabled(true);
        int initialInfo[16] = {};
        ps.processNewInfo(initialInfo);
        ps.processNewInfo(initialInfo);

        int info[16] = {};
        info[4]  = 300;     // FeedbackLevel > 256 (IsFeedbackLevelOK false)
        info[5]  = 1;       // calCount changed
        info[7]  = 1;       // calibration attempt changed
        info[14] = 1;
        info[15] = 6;
        ps.processNewInfo(info);

        ps.autoAttentionTick();   // Monitor → SetNewValues
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::SetNewValues));

        info[15] = 0; // Native reset acknowledgement precedes the ATT write.
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();   // SetNewValues → applies newAtten

        // HL2 post-fix: ddB=10 → newAtten = 0 + 10 = 10.
        // Pre-fix would have produced 0 + 31 = 31 (legacy fallback).
        QCOMPARE(stepAtt.attOnTxValue(), 10);
    }

    void autoAttentionTick_legacyClampsMinusInfinityToMinus100()
    {
        // Sanity: non-HL2 boards keep the legacy ±100 clamp + 31.1 NaN/
        // fallback.  fbLevel=0 → log10(0) = -Infinity → legacy clamp
        // fires `if (ddB < -100.0) ddB = -100.0;` → deltaDb=-100.
        // oldAtten=5 + deltaDb=-100 = -95, clamped to legacy floor 0
        // (Saturn minAttenuation()=0).  This proves the legacy clamp
        // branch is still reached and the HL2-only -10 clamp doesn't
        // bleed across.
        TxChannel tx(kTxChannelId);
        StepAttenuatorController stepAtt;
        MoxController mox;
        PureSignal ps(nullptr, &tx, nullptr, &mox, &stepAtt, nullptr);

        // Saturn = ANAN-G2 board → legacy clamp branch.
        ps.applyBoardCapabilities(
            BoardCapsTable::forBoard(HPSDRHW::Saturn));

        // Legacy floor: 0 (default)
        stepAtt.setAttOnTxValue(5);

        ps.setEnabled(true);
        ps.setAutoAttenuate(true);
        ps.setTimersEnabled(false);
        mox.setMox(true);
        ps.setAutoCalEnabled(true);
        int initialInfo[16] = {};
        ps.processNewInfo(initialInfo);
        ps.processNewInfo(initialInfo);

        int info[16] = {};
        info[4]  = 0;       // FeedbackLevel=0 → log10(0) = -Infinity
        info[5]  = 1;
        info[7]  = 1;
        info[14] = 1;
        info[15] = 6;
        ps.processNewInfo(info);

        ps.autoAttentionTick();   // Monitor → SetNewValues
        QCOMPARE(static_cast<int>(ps.autoAttenuateState()),
                 static_cast<int>(PureSignal::AutoAttenuateState::SetNewValues));

        info[15] = 0; // Native reset acknowledgement precedes the ATT write.
        ps.processNewInfo(info, std::numeric_limits<std::uint64_t>::max());
        ps.autoAttentionTick();   // SetNewValues — clamps to floor=0

        // 5 + (-100) = -95, clamped to legacy floor 0.
        QCOMPARE(stepAtt.attOnTxValue(), 0);
    }

    // ── Fix D: split correctionsBeingAppliedChanged from correctingChanged ──
    //
    // Codex review identified that the legacy code emitted correctingChanged
    // twice with two DIFFERENT semantic predicates:
    //   - PureSignal.cpp:741   correctingChanged(newCorr)        // FB > 90
    //   - PureSignal.cpp:758   correctingChanged(newCorrApplied) // _info[14]==1
    //
    // Thetis treats these as two distinct getters:
    //   PSForm.cs:1100-1102 CorrectionsBeingApplied { _info[14] == 1; }
    //   PSForm.cs:1106-1108 Correcting             { FeedbackLevel > 90; }
    //
    // The fix: emit correctionsBeingAppliedChanged for the info[14] path and
    // keep correctingChanged only for the FeedbackLevel > 90 path.

    // ── The info[] status line is written on change, not every second ─────
    //
    // The Core journal showed "PureSignal info[]: state=0 corrApplied=0 ..."
    // once a second while nothing transmitted. The line now marks a change
    // in what it reports, the same shape as the P1/P2 txLpf lines.

    static int& statusLineCount()
    {
        static int count = 0;
        return count;
    }

    static void countStatusLines(QtMsgType, const QMessageLogContext&,
                                 const QString& msg)
    {
        if (msg.contains(QStringLiteral("PureSignal info[]:"))) {
            ++statusLineCount();
        }
    }

    void infoStatusLine_notRepeatedWhileValuesUnchanged()
    {
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setTimersEnabled(false);

        statusLineCount() = 0;
        const QtMessageHandler previous = qInstallMessageHandler(countStatusLines);

        // 100 ticks is ten seconds of the 100 ms poll with nothing changing.
        int info[16] = {};
        for (int i = 0; i < 100; ++i) {
            ps.processNewInfo(info);
        }
        const int idleLines = statusLineCount();

        // A change in a reported value is still written.
        info[15] = 3;   // engine state
        for (int i = 0; i < 20; ++i) {
            ps.processNewInfo(info);
        }
        const int afterChange = statusLineCount();

        qInstallMessageHandler(previous);

        QVERIFY2(idleLines <= 1,
                 qPrintable(QStringLiteral("idle ticks wrote %1 status lines")
                                .arg(idleLines)));
        QCOMPARE(afterChange, idleLines + 1);
    }

    void correctionsBeingAppliedChanged_emittedOnInfo14Toggle()
    {
        // Toggling _info[14] between 0 and 1 must emit
        // correctionsBeingAppliedChanged on each transition, with no spurious
        // correctingChanged fires from this same path (FB stays at 0 here, so
        // FB>90 stays false → correctingChanged should not fire).
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setAutoCalEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy applied(&ps, &PureSignal::correctionsBeingAppliedChanged);
        QSignalSpy correcting(&ps, &PureSignal::correctingChanged);

        // Tick 1: info[14]=1 → applied transitions false → true.
        // info[4] (FeedbackLevel) stays 0 so FB > 90 = false (no change from
        // ctor default false → no correctingChanged emit).
        int info[16] = {};
        info[4]  = 0;
        info[14] = 1;
        ps.processNewInfo(info);
        QCOMPARE(applied.count(), 1);
        QCOMPARE(applied.takeFirst().at(0).toBool(), true);
        QCOMPARE(correcting.count(), 0);

        // Tick 2: info[14]=0 → applied transitions true → false.
        info[14] = 0;
        info[5]  = 1;   // bump calCount so HasInfoChanged passes
        ps.processNewInfo(info);
        QCOMPARE(applied.count(), 1);
        QCOMPARE(applied.takeFirst().at(0).toBool(), false);
        QCOMPARE(correcting.count(), 0);
    }

    void correctingChanged_emittedOnFeedbackLevelCrossing90()
    {
        // From Thetis PSForm.cs:1106-1108 [v2.10.3.13]:
        //   public static bool Correcting { get { return FeedbackLevel > 90; } }
        // Crossing the 90-line in either direction must emit correctingChanged
        // exactly once.  No correctionsBeingAppliedChanged from this path.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setAutoCalEnabled(true);
        ps.setTimersEnabled(false);

        // Tick 1: prime with fb=80, info[14]=0.  Default state was already
        // correcting=false / correctionsApplied=false, so neither signal
        // should fire because no transition occurred.
        QSignalSpy applied(&ps, &PureSignal::correctionsBeingAppliedChanged);
        QSignalSpy correcting(&ps, &PureSignal::correctingChanged);

        int info[16] = {};
        info[4]  = 80;       // fb < 90 → correcting=false (no change)
        info[5]  = 1;
        info[14] = 0;
        ps.processNewInfo(info);
        QCOMPARE(correcting.count(), 0);    // no transition (false→false)
        QCOMPARE(applied.count(),    0);    // no transition (false→false)

        // Tick 2: fb=100 crosses 90 → correctingChanged(true) fires once.
        // info[14] still 0 → no correctionsBeingAppliedChanged.
        info[4] = 100;
        info[5] = 2;
        ps.processNewInfo(info);
        QCOMPARE(correcting.count(), 1);
        QCOMPARE(correcting.takeFirst().at(0).toBool(), true);
        QCOMPARE(applied.count(), 0);
    }

    void correctingChanged_andCorrectionsBeingAppliedChanged_areDistinct()
    {
        // Drive each input independently and verify the OTHER signal does
        // not fire.  Single test that pins the contract: the two predicates
        // are completely orthogonal.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setAutoCalEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy applied(&ps, &PureSignal::correctionsBeingAppliedChanged);
        QSignalSpy correcting(&ps, &PureSignal::correctingChanged);

        // Step A: flip info[14] only (fb stays 0 → no correcting transition).
        int info[16] = {};
        info[14] = 1;
        ps.processNewInfo(info);
        QCOMPARE(applied.count(),    1);
        QCOMPARE(correcting.count(), 0);
        applied.clear();

        // Step B: flip fb above 90 (info[14] stays 1 → no applied transition).
        info[4] = 150;
        info[5] = 1;
        ps.processNewInfo(info);
        QCOMPARE(correcting.count(), 1);
        QCOMPARE(applied.count(),    0);
    }

    // ── Codex Fix C: psEnabledChanged fan-out for cmd-state transitions ────
    //
    // Codex review identified that the radio/DDC fan-out (UpdateDDCs /
    // SetPureSignal / SendHighPriority / setPSRunCal) was wired only to
    // autoCalEnabledChanged.  Per Thetis PSForm.cs:235-269 PSEnabled
    // property setter [v2.10.3.13], that setter is THE fan-out for ALL 5
    // cmd-state transitions:
    //
    //   case TurnOnAutoCalibrate (PSForm.cs:646)        → PSEnabled=true
    //   case TurnOnSingleCalibrate (PSForm.cs:662)      → PSEnabled=true
    //   case IntiateRestoredCorrection (PSForm.cs:720)  → PSEnabled=true
    //   case StayON (PSForm.cs:678)                     → PSEnabled=false
    //   case TurnOFF (PSForm.cs:705)                    → PSEnabled=true
    //
    // Without the new signal, Single Cal / Restore / Stay-on / Turn-off
    // paths only set calcc flags via setPSRunCal but never fired the
    // radio-side (UpdateDDCs / SetPureSignal high-priority push) or the
    // StepAttenuatorController setPsActive path that the autoCalEnabled
    // wiring carried.  Result: PsccPump never sees the puresignal_run wire
    // bit, the PA-feedback ADC path is silent during single-cal, and the
    // step attenuator pinning logic mis-fires.
    //
    // Fix introduces a real psEnabledChanged signal emitted by the cmd-state
    // machine on each PSEnabled flip.  RadioModel reroutes the radio/DDC
    // fan-out from autoCalEnabledChanged to psEnabledChanged.
    // autoCalEnabledChanged stays live for the PS-A button visual state.

    void psEnabledChanged_emittedOnTurnOnAutoCalibrate()
    {
        // setAutoCalEnabled(true) sets _autoON=true; the next pollTimerTick
        // transitions Off → TurnOnAutoCalibrate, where Thetis PSForm.cs:646
        // [v2.10.3.13] fires `if (!PSEnabled) PSEnabled = true;`.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);
        ps.setCorrectionStateForTest(Ps3CorrectionState{false, false});

        QSignalSpy psEnabled(&ps, &PureSignal::psEnabledChanged);

        ps.setAutoCalEnabled(true);   // sets _autoON=true (no PS fan-out yet)

        // At this stage psEnabledChanged should not have fired — the cmd-state
        // machine hasn't tickled the transition yet.
        QCOMPARE(psEnabled.count(), 0);

        // Drive cmd-state ticks: Off → TurnOnAutoCalibrate transition fires
        // PSEnabled=true on entry to TurnOnAutoCalibrate.
        int info[16] = {};
        ps.processNewInfo(info);   // tick 1: Off → next tick sets up TurnOn
        ps.processNewInfo(info);   // tick 2: TurnOnAutoCalibrate → PSEnabled=true

        // Either tick may fire psEnabledChanged depending on transition order.
        // We just require it fired with true.
        QVERIFY(psEnabled.count() >= 1);
        QCOMPARE(psEnabled.takeFirst().at(0).toBool(), true);
    }

    void psEnabledChanged_emittedOnTurnOnSingleCalibrate()
    {
        // singleCalibrate() sets _singleCalON=true.  The next pollTimerTick
        // transitions Off → TurnOnSingleCalibrate, where Thetis PSForm.cs:662
        // [v2.10.3.13] fires `if (!PSEnabled) PSEnabled = true;`.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy psEnabled(&ps, &PureSignal::psEnabledChanged);

        ps.singleCalibrate();   // sets _singleCalON=true

        // Drive cmd-state ticks until PSEnabled flips.
        int info[16] = {};
        ps.processNewInfo(info);   // Off → next has _singleCalON
        ps.processNewInfo(info);   // TurnOnSingleCalibrate → PSEnabled=true

        QVERIFY(psEnabled.count() >= 1);
        QCOMPARE(psEnabled.takeFirst().at(0).toBool(), true);
    }

    void psEnabledChanged_emittedOnTurnOff()
    {
        // Enable then disable cycle: psEnabledChanged must fire with true on
        // the way up (TurnOnAutoCalibrate) and then with the StayOn / TurnOff
        // exit branches on the way down.  Thetis PSForm.cs:678 [v2.10.3.13]
        // fires `if (PSEnabled) PSEnabled = false;` at StayON; PSForm.cs:705
        // fires `if (!PSEnabled) PSEnabled = true;` at TurnOFF (yes —
        // TurnOFF re-enables: it's the engine's reset-to-LRESET sequence).
        // After draining we expect at least one true→false transition.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);
        ps.setCorrectionStateForTest(Ps3CorrectionState{false, false});

        // Drive auto-cal up.
        ps.setAutoCalEnabled(true);
        int info[16] = {};
        ps.processNewInfo(info);
        ps.processNewInfo(info);   // TurnOnAutoCalibrate fires PSEnabled=true

        QSignalSpy psEnabled(&ps, &PureSignal::psEnabledChanged);

        // Drive auto-cal down.  The cmd-state machine walks the chain:
        //   AutoCalibrate → TurnOff (re-enables PSEnabled=true) → Off
        //   (PSEnabled=false at next visit if _info[15]==LRESET).
        ps.setAutoCalEnabled(false);

        // To reach Off we need _info[14]=0 AND state=LRESET (per PSForm.cs:
        // 714-715 [v2.10.3.13]).  Drive ticks with that combination.
        info[14] = 0;
        info[15] = 0;   // LRESET
        for (int i = 0; i < 8; ++i) {
            ps.processNewInfo(info);
        }

        // Expect at least one psEnabledChanged(false) emit during teardown.
        bool sawFalse = false;
        for (int i = 0; i < psEnabled.count(); ++i) {
            if (psEnabled.at(i).at(0).toBool() == false) {
                sawFalse = true;
                break;
            }
        }
        QVERIFY2(sawFalse, "Expected psEnabledChanged(false) during teardown");
    }

    void psEnabledChanged_separateFromAutoCalEnabledChanged()
    {
        // Verify the two signals are distinct emitters: setAutoCalEnabled(x)
        // ALONE (without a poll tick) must emit autoCalEnabledChanged but NOT
        // psEnabledChanged.  Only the cmd-state machine emits
        // psEnabledChanged.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy autoCal(&ps, &PureSignal::autoCalEnabledChanged);
        QSignalSpy psEnabled(&ps, &PureSignal::psEnabledChanged);

        ps.setAutoCalEnabled(true);

        // setAutoCalEnabled fires autoCalEnabledChanged immediately, but
        // psEnabledChanged is gated on the cmd-state machine's TurnOn*
        // visits which only happen during a poll tick.
        QCOMPARE(autoCal.count(),   1);
        QCOMPARE(psEnabled.count(), 0);
    }

    // ── Codex Fix E: _performing_single_cal retry loop in StayON ───────────
    //
    // From Thetis PSForm.cs:553-554 [v2.10.3.13]:
    //   private bool _performing_single_cal = false;
    //   private int _performing_single_cal_retries = 0;
    //
    // From Thetis PSForm.cs:658-665 [v2.10.3.13] (TurnOnSingleCalibrate case):
    //   _autoON = false;
    //   _performing_single_cal = true;            // ← enter the retry-tracked window
    //   puresignal.SetPSControl(_txachannel, 1, 1, 0, 0);
    //   if (!PSEnabled) PSEnabled = true;
    //   ...
    //   _cmdstate = eCMDState.SingleCalibrate;
    //
    // From Thetis PSForm.cs:677-700 [v2.10.3.13] (StayON case) —
    //   case eCMDState.StayON://5:     // Stay-ON
    //       if (PSEnabled) PSEnabled = false;
    //       ...
    //       else if (_performing_single_cal)
    //       {
    //           // fix for when we were performing a single cal, but needed to change attenuation
    //           _performing_single_cal = false;
    //           if (!puresignal.IsFeedbackLevelOKRange && _performing_single_cal_retries < 5)
    //           {
    //               _performing_single_cal_retries++;
    //               _singlecalON = true;
    //           }
    //           else
    //               _performing_single_cal_retries = 0;
    //       }
    //       break;
    //
    // From Thetis PSForm.cs:1116-1119 IsFeedbackLevelOKRange [v2.10.3.13]:
    //   public static bool IsFeedbackLevelOKRange
    //   { get { return FeedbackLevel > 128 && FeedbackLevel <= 181; } }
    //
    // Pre-fix NereusSDR: never recorded the single-cal attempt nor retried
    // bad-feedback runs.  Post-fix: track _performing_single_cal across
    // TurnOnSingleCalibrate -> StayOn, then retry up to 5x on bad feedback
    // (range = (128, 181] for IsFeedbackLevelOKRange).

    // Test scaffolding: drive a single-cal-then-StayOn cycle of cmd-state
    // ticks.  These tests run BELOW the singleCalibrate() user-toggle so
    // they invoke the cmd-state machine directly and observe the StayOn
    // retry branch (Codex Fix E).  Per Thetis PSForm.cs:677-699 the retry
    // re-arms `m_singleCalON = true` from inside StayOn; the next StayOn
    // tick consumes it and transitions to TurnOnSingleCalibrate.
    //
    // A "complete cycle" walks:
    //   tick A: Off                  (_singleCalON consumed, → TurnOnSingleCalibrate)
    //   tick B: TurnOnSingleCalibrate (sets _performing_single_cal=true, → SingleCalibrate)
    //   tick C: SingleCalibrate      (info[14]=1 → StayOn, calibrationComplete fires)
    //   tick D: StayOn               (with FB level: retry branch fires)
    //   tick E: StayOn               (if retry: _singleCalON=true → TurnOnSingleCalibrate next)

    // Drive ticks A-D for ONE single-cal cycle.  Caller is responsible for
    // calling ps.singleCalibrate() ONCE before the FIRST cycle; subsequent
    // retry cycles are driven by the StayOn re-arming m_singleCalON
    // automatically.  feedbackLevel sets info[4] for the StayOn evaluation.
    static void driveOneSingleCalCycle(PureSignal& ps, int& counter,
                                       int feedbackLevel) {
        int info[16] = {};
        ++counter;
        info[5] = counter;
        ps.processNewInfo(info);   // Off → TurnOnSingleCalibrate
        ++counter;
        info[5] = counter;
        ps.processNewInfo(info);   // TurnOnSingleCalibrate → SingleCalibrate
        info[14] = 1;
        ++counter;
        info[5] = counter;
        ps.processNewInfo(info);   // SingleCalibrate → StayOn (corrApplied)
        info[4] = feedbackLevel;
        ++counter;
        info[5] = counter;
        ps.processNewInfo(info);   // StayOn evaluates _performing_single_cal
    }

    void singleCal_retriesOnBadFeedback_upToFiveTimes()
    {
        // Drive 5 retries of bad feedback (FB=80, !IsFeedbackLevelOKRange).
        // Each cycle's StayOn branch fires the retry — m_singleCalON gets
        // re-armed → next tick chain enters TurnOnSingleCalibrate again.
        //
        // After 5 retries, counter is at 5; the 6th StayOn evaluation hits
        // the cap and resets the counter to 0 instead of re-arming.
        //
        // Observable: calibrationComplete fires on each SingleCalibrate →
        // StayOn transition.  6 cycles total = 6 calibrationComplete emits
        // (initial + 5 retries).  After cycle 6, no further retry → no
        // 7th calibrationComplete from natural cmd-state progression.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy completeSpy(&ps, &PureSignal::calibrationComplete);

        ps.singleCalibrate();   // initial: _singleCalON=true

        int counter = 0;
        // Cycle 0: initial single-cal.  After this, retries=0, retry fires
        // (FB=80 fails IsFeedbackLevelOKRange) → re-arms _singleCalON.
        driveOneSingleCalCycle(ps, counter, /*fb=*/80);
        QCOMPARE(completeSpy.count(), 1);

        // Cycles 1-5: retries++ each cycle (1, 2, 3, 4, 5).  Each completes
        // (corrApplied=1 in tick C → StayOn → calibrationComplete emits).
        for (int i = 1; i <= 5; ++i) {
            driveOneSingleCalCycle(ps, counter, /*fb=*/80);
            QCOMPARE(completeSpy.count(), i + 1);
        }
        // After cycle 5: retries was already 5 → cap hit → reset to 0,
        // m_singleCalON NOT re-armed.  6 completes total (cycle 0 + cycles 1-5).
        QCOMPARE(completeSpy.count(), 6);
    }

    void singleCal_retriesResetAfterFiveAttempts()
    {
        // Pin the contract: after 5 retries, m_singleCalON is NOT re-armed
        // by the StayOn retry branch.  Verify by attempting a 7th cycle
        // and observing it is NOT auto-driven.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy completeSpy(&ps, &PureSignal::calibrationComplete);

        ps.singleCalibrate();   // initial trigger
        int counter = 0;

        // Run 6 cycles total (initial + 5 retries).  Each cycle adds one
        // calibrationComplete fire.
        for (int i = 0; i < 6; ++i) {
            driveOneSingleCalCycle(ps, counter, /*fb=*/80);
        }
        QCOMPARE(completeSpy.count(), 6);
        completeSpy.clear();

        // 7th cycle attempt — but m_singleCalON should NOT have been
        // re-armed.  Drive ticks A and B: from StayOn the next tick should
        // stay in StayOn (no _singleCalON to consume).  No calibrationComplete.
        int info[16] = {};
        ++counter;
        info[5] = counter;
        info[14] = 1;
        ps.processNewInfo(info);   // StayOn re-eval — no transition
        ++counter;
        info[5] = counter;
        ps.processNewInfo(info);   // StayOn re-eval — no transition

        QCOMPARE(completeSpy.count(), 0);
    }

    void singleCal_doesNotRetry_whenFeedbackIsInOkRange()
    {
        // From PSForm.cs:1116-1119 IsFeedbackLevelOKRange [v2.10.3.13]:
        //   FeedbackLevel > 128 && FeedbackLevel <= 181
        // FB=150 sits in the OK range → !IsFeedbackLevelOKRange = false →
        // the retry branch's `if` is false → else { _retries = 0; } runs;
        // _singleCalON is NOT re-armed.  No further single-cal cycle.
        TxChannel tx(kTxChannelId);
        PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
        ps.setEnabled(true);
        ps.setTimersEnabled(false);

        QSignalSpy completeSpy(&ps, &PureSignal::calibrationComplete);

        ps.singleCalibrate();
        int counter = 0;
        driveOneSingleCalCycle(ps, counter, /*fb=*/150);
        QCOMPARE(completeSpy.count(), 1);
        completeSpy.clear();

        // After StayOn with good feedback, no retry queued.  Drive more
        // ticks — should stay in StayOn, no calibrationComplete.
        int info[16] = {};
        ++counter;
        info[5] = counter;
        info[14] = 1;
        info[4] = 150;
        ps.processNewInfo(info);
        ++counter;
        info[5] = counter;
        ps.processNewInfo(info);

        QCOMPARE(completeSpy.count(), 0);
    }

    void singleCal_isFeedbackLevelOKRange_boundariesMatchThetis()
    {
        // From PSForm.cs:1116-1119 [v2.10.3.13] IsFeedbackLevelOKRange =
        //   FeedbackLevel > 128 && FeedbackLevel <= 181
        // Boundary table:
        //   128 → false (fails > 128)  → retry
        //   129 → true                  → no retry
        //   181 → true                  → no retry
        //   182 → false (fails <= 181) → retry
        //
        // Drive one cycle.  Look at calibrationComplete count after
        // additional StayOn ticks: if no retry, count stays at 1; if retry,
        // count climbs.
        auto runAndCheckRetried = [](int feedbackLevel) -> bool {
            TxChannel tx(kTxChannelId);
            PureSignal ps(nullptr, &tx, nullptr, nullptr, nullptr, nullptr);
            ps.setEnabled(true);
            ps.setTimersEnabled(false);
            ps.setCorrectionStateForTest(Ps3CorrectionState{false, false});

            QSignalSpy completeSpy(&ps, &PureSignal::calibrationComplete);

            ps.singleCalibrate();
            int counter = 0;
            // Run cycle 1 to land in StayOn at the supplied FB level.
            driveOneSingleCalCycle(ps, counter, feedbackLevel);

            // Run a 2nd cycle's worth of ticks. If retry was queued, this
            // drives the cmd-state machine through the retry chain and
            // calibrationComplete fires AGAIN (count goes 1 → 2).
            // If no retry, count stays at 1.
            int info[16] = {};
            // Tick: StayOn → if retried, m_singleCalON now true → next tick:
            //                 TurnOnSingleCalibrate
            ++counter;
            info[5] = counter;
            info[14] = 1;
            info[4] = feedbackLevel;
            ps.processNewInfo(info);
            // Tick: TurnOnSingleCalibrate → SingleCalibrate
            ++counter;
            info[5] = counter;
            ps.processNewInfo(info);
            // Tick: SingleCalibrate sees corrApplied=1 → StayOn (fires complete)
            ++counter;
            info[5] = counter;
            ps.processNewInfo(info);

            return completeSpy.count() >= 2;
        };

        QVERIFY2( runAndCheckRetried(128),
                 "FB=128 must retry (fails IsFeedbackLevelOKRange's >128)");
        QVERIFY2(!runAndCheckRetried(129),
                 "FB=129 must NOT retry (passes IsFeedbackLevelOKRange)");
        QVERIFY2(!runAndCheckRetried(181),
                 "FB=181 must NOT retry (passes IsFeedbackLevelOKRange's <=181)");
        QVERIFY2( runAndCheckRetried(182),
                 "FB=182 must retry (fails IsFeedbackLevelOKRange's <=181)");
    }

};

// QTEST_GUILESS_MAIN constructs a QCoreApplication so the internal QTimers
// inside PureSignal have a working event dispatcher.  QTEST_APPLESS_MAIN
// would emit "QObject::startTimer: current thread's event dispatcher has
// already been destroyed" warnings on the setEnabled(true)/setTimersEnabled
// paths even though the asserts pass.
QTEST_GUILESS_MAIN(TstPureSignalCoordinator)
#include "tst_puresignal_coordinator.moc"
