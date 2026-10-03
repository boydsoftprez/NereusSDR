// no-port-check: NereusSDR-original unit-test file.  Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_psa_indicator.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the Phase 3M-4 Task 10 PsaIndicatorWidget.
//
// PsaIndicatorWidget is a 1:1 port of Thetis ucInfoBar.cs:820-1098
// [v2.10.3.13] — the lblFB / lblPS bottom-banner sub-controls plus
// updatePSDisplay state machine, lblFB_MouseDown click handlers, and
// setToolTips dynamic tooltip.  The test file exercises:
//
//   1. The widget constructs with a null RadioModel pointer (test seam).
//   2. 6-state machine: PS off, PS on no MOX, TX hide-feedback, TX
//      numeric feedback, TX correcting, TX corrections-applied not
//      correcting.  Each state verified for FB text + PS text + FB
//      background color + PS background color.
//   3. Color thresholds (PSForm.cs:1123-1138 [v2.10.3.13]):
//      - 0..90 default → Red, swapped → DodgerBlue
//      - 91..128 → Yellow (always)
//      - 129..181 → Lime (always)
//      - 182+ default → DodgerBlue, swapped → Red
//   4. Click handlers (ucInfoBar.cs:1042-1054 [v2.10.3.13]):
//      - Left-click on FB emits invertRedBlueRequested
//      - Right-click on FB emits hideFeedbackToggleRequested
//      - Click on PS label is passive (no signal emitted)
//   5. Compact-fonts threshold (_useSmallFonts):
//      - "FB" instead of "Feedback"
//      - "Correct" instead of "Correcting"
//   6. Tooltip text (ucInfoBar.cs:1081-1096 [v2.10.3.13]):
//      - Default un-swapped: "Red 0-90 ... Blue 182+"
//      - Swapped: "Blue 0-90 ... Red 182+"
//      - "Showing level, " prefix when !hideFeedback
//
// Source: NereusSDR-original.  See PsaIndicatorWidget.h for the Thetis
// cite map.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — New test file for Phase 3M-4 Task 10: PsaIndicatorWidget
//                 unit tests.  J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QLabel>
#include <QSignalSpy>

#include "gui/PsaIndicatorWidget.h"

using namespace NereusSDR;

// Reference colors — System.Drawing exact RGBs as documented in the
// PsaIndicatorWidget palette comment block.  Kept here so failure
// messages report named values instead of hex literals.
static const QColor kDimGray(105, 105, 105);
static const QColor kSeaGreen( 46, 139,  87);
static const QColor kLime(     0, 255,   0);
static const QColor kYellow( 255, 255,   0);
static const QColor kRed(    255,   0,   0);
static const QColor kDodgerBlue(30, 144, 255);

class TstPsaIndicator : public QObject {
    Q_OBJECT

private slots:

    // ── Test 1: construct + destruct without any wiring ─────────────────────

    void constructAndDestruct_withNoModel_doesNotCrash()
    {
        PsaIndicatorWidget w(nullptr);
        QCOMPARE(w.fbText(), QStringLiteral("Feedback"));
        QCOMPARE(w.psText(), QStringLiteral("PureSignal 3"));
    }

    // ── State 1: PS off → DimGray / DimGray, FB "Feedback" ───────────────────

    void psOffState_showsDimGray_withFeedbackText()
    {
        // Source: ucInfoBar.cs:841-851 [v2.10.3.13] — !_psEnabled branch.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(false);
        QCOMPARE(w.fbBackgroundColor(), kDimGray);
        QCOMPARE(w.psBackgroundColor(), kDimGray);
        QCOMPARE(w.fbText(), QStringLiteral("Feedback"));
        QCOMPARE(w.psText(), QStringLiteral("PureSignal 3"));
    }

    // ── State 2: PS on, no MOX → SeaGreen / SeaGreen, FB "Feedback" ──────────

    void psOnRxIdle_showsSeaGreen_withFeedbackText()
    {
        // Source: ucInfoBar.cs:882-897 [v2.10.3.13] — _psEnabled && !_mox branch.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(false);
        QCOMPARE(w.fbBackgroundColor(), kSeaGreen);
        QCOMPARE(w.psBackgroundColor(), kSeaGreen);
        QCOMPARE(w.fbText(), QStringLiteral("Feedback"));
        QCOMPARE(w.psText(), QStringLiteral("PureSignal 3"));
    }

    // ── State 3: TX, hide-feedback → FB "Feedback" (text), color tracks level ─

    void psOnTxHideFeedback_showsFeedbackText()
    {
        // Source: ucInfoBar.cs:870-876 [v2.10.3.13] — _hideFeedback branch.
        // FB color still reflects feedback level (not text) so user sees
        // the calibration health via color alone.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setFeedbackLevel(150);   // in-range → Lime FB color
        w.setHideFeedback(true);
        QCOMPARE(w.fbText(), QStringLiteral("Feedback"));
        QCOMPARE(w.fbBackgroundColor(), kLime);
        QCOMPARE(w.psText(), QStringLiteral("PureSignal 3"));
    }

    // ── State 4: TX, !hideFeedback, cal-changed → FB shows numeric level ─────

    void psOnTxNumericFeedback_showsLevelAsString()
    {
        // Source: ucInfoBar.cs:877-880 [v2.10.3.13] — numeric path.
        // Phase 3M-4 bench-fix Round 2: m_calChangedSinceLastDraw is now
        // driven exclusively by psInfo()'s calibrationAttemptsChanged
        // parameter (matches Thetis ucInfoBar.cs:812 [v2.10.3.13] PSInfo
        // body — the flag was previously bumped on every setFeedbackLevel
        // call, which was a NereusSDR-only deviation that the bench user
        // tripped on when the FB label showed "Feedback" instead of the
        // numeric value).  Tests now drive the slot directly.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setHideFeedback(false);
        w.psInfo(/*level=*/163, /*ok=*/true, /*corrApp=*/true,
                 /*calChanged=*/true, kLime);
        QCOMPARE(w.fbText(), QStringLiteral("163"));
    }

    // ── State 5: TX + corrections-applied → PS Lime "Correcting" ────────────

    void psOnTxCorrecting_showsLimeAndCorrectingText()
    {
        // Source: ucInfoBar.cs:856-860 updatePSDisplay [v2.10.3.13]:
        //   if (_bCorrectionsBeingApplied) {
        //       lblPS.Text = "Correcting"; lblPS.BackColor = Lime;
        //   }
        // Phase 3M-4 bench-fix Round 2: dropped the NereusSDR-only
        // m_correcting nested branch.  Lime/SeaGreen split is decided
        // purely by m_correctionsApplied per upstream byte-for-byte.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setCorrectionsBeingApplied(true);
        w.setFeedbackLevel(150);
        QCOMPARE(w.psText(), QStringLiteral("Correcting"));
        QCOMPARE(w.psBackgroundColor(), kLime);
    }

    // ── State 6: TX + NOT corrections-applied → SeaGreen "Pure Signal2" ─────

    void psOnTxCorrectionsAppliedFalse_showsSeaGreen()
    {
        // Source: ucInfoBar.cs:861-865 updatePSDisplay [v2.10.3.13] —
        // !_bCorrectionsBeingApplied else branch.  When the calcc engine is
        // not actively applying corrections (info[14] == 0 per
        // PSForm.cs:1100-1102 [v2.10.3.13] CorrectionsBeingApplied), the
        // PS label drops back to the SeaGreen "Pure Signal2" idle look.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setCorrectionsBeingApplied(false);
        QCOMPARE(w.psText(), QStringLiteral("PureSignal 3"));
        QCOMPARE(w.psBackgroundColor(), kSeaGreen);
    }

    // ── Color thresholds: 4 cardinal points + swap behavior ──────────────────

    void feedbackUnder90_default_showsRed()
    {
        // Source: PSForm.cs:1132-1136 [v2.10.3.13] — under-range default.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setInvertRedBlue(false);
        w.setFeedbackLevel(45);
        QCOMPARE(w.fbBackgroundColor(), kRed);
    }

    void feedbackUnder90_swapped_showsDodgerBlue()
    {
        // Source: PSForm.cs:1133-1134 [v2.10.3.13] — under-range swapped.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setInvertRedBlue(true);
        w.setFeedbackLevel(45);
        QCOMPARE(w.fbBackgroundColor(), kDodgerBlue);
    }

    void feedbackInMarginalRange_showsYellow()
    {
        // Source: PSForm.cs:1131 [v2.10.3.13] — 91..128 = Yellow.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setFeedbackLevel(120);
        QCOMPARE(w.fbBackgroundColor(), kYellow);
    }

    void feedbackInTargetRange_showsLime()
    {
        // Source: PSForm.cs:1130 [v2.10.3.13] — 129..181 = Lime.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setFeedbackLevel(150);
        QCOMPARE(w.fbBackgroundColor(), kLime);
    }

    void feedbackOver181_default_showsDodgerBlue()
    {
        // Source: PSForm.cs:1126-1129 [v2.10.3.13] — over-range default.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setInvertRedBlue(false);
        w.setFeedbackLevel(220);
        QCOMPARE(w.fbBackgroundColor(), kDodgerBlue);
    }

    void feedbackOver181_swapped_showsRed()
    {
        // Source: PSForm.cs:1127 [v2.10.3.13] — over-range swapped.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setInvertRedBlue(true);
        w.setFeedbackLevel(220);
        QCOMPARE(w.fbBackgroundColor(), kRed);
    }

    // ── Click handlers ───────────────────────────────────────────────────────

    void leftClickOnFb_emitsInvertRedBlueRequested()
    {
        // Source: ucInfoBar.cs:1044-1048 [v2.10.3.13] — Left mouse button
        // toggles SwapRedBlue.
        PsaIndicatorWidget w(nullptr);
        w.show();
        w.resize(200, 20);
        QSignalSpy spy(&w, &PsaIndicatorWidget::invertRedBlueRequested);
        w.simulateLeftClickOnFb();
        QCOMPARE(spy.count(), 1);
    }

    void rightClickOnFb_emitsHideFeedbackToggleRequested()
    {
        // Source: ucInfoBar.cs:1049-1053 [v2.10.3.13] — Right mouse button
        // toggles HideFeedback.
        PsaIndicatorWidget w(nullptr);
        w.show();
        w.resize(200, 20);
        QSignalSpy spy(&w, &PsaIndicatorWidget::hideFeedbackToggleRequested);
        w.simulateRightClickOnFb();
        QCOMPARE(spy.count(), 1);
    }

    void clickOnPs_emitsNothing()
    {
        // Per Thetis ucInfoBar.cs — there is no lblPS_MouseDown handler.
        // PS label is passive; click should not emit either signal.
        PsaIndicatorWidget w(nullptr);
        w.show();
        w.resize(200, 20);
        QSignalSpy invertSpy(&w, &PsaIndicatorWidget::invertRedBlueRequested);
        QSignalSpy hideSpy(&w, &PsaIndicatorWidget::hideFeedbackToggleRequested);
        w.simulateLeftClickOnPs();
        w.simulateRightClickOnPs();
        QCOMPARE(invertSpy.count(), 0);
        QCOMPARE(hideSpy.count(), 0);
    }

    // ── Compact fonts ────────────────────────────────────────────────────────

    void useSmallFonts_collapsesFeedbackLabel()
    {
        // Source: ucInfoBar.cs:846-849 [v2.10.3.13] — _useSmallFonts gate
        // for "FB" vs "Feedback".  PS off branch is the simplest way to
        // observe the toggle without depending on cal-state.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(false);
        w.setUseSmallFonts(true);
        QCOMPARE(w.fbText(), QStringLiteral("FB"));
    }

    void useSmallFonts_collapsesCorrectingLabel()
    {
        // Source: ucInfoBar.cs:858 [v2.10.3.13] — "Correct" vs "Correcting".
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.setCorrectionsBeingApplied(true);
        w.setUseSmallFonts(true);
        QCOMPARE(w.psText(), QStringLiteral("Correct"));
    }

    // ── Tooltip text ─────────────────────────────────────────────────────────

    void tooltipDefault_matchesThetis()
    {
        // Source: ucInfoBar.cs:1093-1094 [v2.10.3.13] — un-swapped path:
        //   "Red 0-90, Yellow 91-128, Green 129-181, Blue 182+".
        PsaIndicatorWidget w(nullptr);
        w.setHideFeedback(true);     // strip "Showing level, " prefix
        w.setInvertRedBlue(false);
        QLabel* fb = w.findChild<QLabel*>(QStringLiteral("lblFB"));
        QVERIFY(fb != nullptr);
        QCOMPARE(fb->toolTip(),
                 QStringLiteral("Red 0-90, Yellow 91-128, "
                                "Green 129-181, Blue 182+"));
    }

    void tooltipSwapped_matchesThetis()
    {
        // Source: ucInfoBar.cs:1090 [v2.10.3.13] — swapped path:
        //   "Blue 0-90, Yellow 91-128, Green 129-181, Red 182+".
        PsaIndicatorWidget w(nullptr);
        w.setHideFeedback(true);     // strip "Showing level, " prefix
        w.setInvertRedBlue(true);
        QLabel* fb = w.findChild<QLabel*>(QStringLiteral("lblFB"));
        QVERIFY(fb != nullptr);
        QCOMPARE(fb->toolTip(),
                 QStringLiteral("Blue 0-90, Yellow 91-128, "
                                "Green 129-181, Red 182+"));
    }

    void tooltipPrefix_appearsWhenNotHidingFeedback()
    {
        // Source: ucInfoBar.cs:1085-1086 [v2.10.3.13] — "Showing level, "
        // prefix appears only when !HideFeedback.
        PsaIndicatorWidget w(nullptr);
        w.setHideFeedback(false);
        w.setInvertRedBlue(false);
        QLabel* fb = w.findChild<QLabel*>(QStringLiteral("lblFB"));
        QVERIFY(fb != nullptr);
        QVERIFY(fb->toolTip().startsWith(QStringLiteral("Showing level, ")));
    }

    // ── Phase 3M-4 bench-fix Round 2: psInfo() consolidated slot ───────────

    void psInfo_withMoxAndCalChanged_showsNumericFeedbackAndLime()
    {
        // Source: ucInfoBar.cs:808-825 PSInfo() [v2.10.3.13].
        // calibrationAttemptsChanged && _mox is the gate that pushes all
        // 5 fields into the widget.  After this call:
        //   _nFeedbackLevel = 150
        //   _bCorrectionsBeingApplied = true
        //   _bCalibrationAttemptsChanged = true
        // updatePSDisplay then renders the numeric path:
        //   PS  → Lime "Correcting"
        //   FB  → 150 (numeric since !_hideFeedback && _bCalibrationAttempts)
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.psInfo(/*level=*/150, /*ok=*/true, /*corrApp=*/true,
                 /*calChanged=*/true, kLime);
        QCOMPARE(w.fbText(), QStringLiteral("150"));
        QCOMPARE(w.psText(), QStringLiteral("Correcting"));
        QCOMPARE(w.psBackgroundColor(), kLime);
    }

    void psInfo_withMoxAndCalChangedFalse_doesNotUpdateFields()
    {
        // Source: ucInfoBar.cs:814 [v2.10.3.13]:
        //   if (_bCalibrationAttemptsChanged && _mox) { ... updatePSDisplay(); }
        // When calChanged is false, the inner block is skipped entirely —
        // _nFeedbackLevel + _bCorrectionsBeingApplied stay at their prior
        // values.  We verify by seeding state via a calChanged=true call,
        // then checking that calChanged=false leaves the fields untouched.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.psInfo(150, true, true, true, kLime);    // seed state
        QCOMPARE(w.fbText(), QStringLiteral("150"));
        QCOMPARE(w.psText(), QStringLiteral("Correcting"));

        // Second call with calChanged=false: the `if (calChanged && mox)`
        // gate fails, so _nFeedbackLevel + _bCorrectionsBeingApplied keep
        // their prior values and updatePSDisplay() is NOT called.  The
        // sticky state survives.
        w.psInfo(50, true, false, false, kRed);
        QCOMPARE(w.fbText(), QStringLiteral("150"));   // sticky
        QCOMPARE(w.psText(), QStringLiteral("Correcting")); // sticky
    }

    void psInfo_withoutMox_doesNotUpdateFields()
    {
        // Source: ucInfoBar.cs:814 [v2.10.3.13] — `&& _mox` half of the
        // gate.  PSInfo() always sets _bCalibrationAttemptsChanged but
        // only consumes the rest when _mox is true.  Verify by calling
        // psInfo() before setMox(true) is issued.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        // m_mox stays at default (false)
        w.psInfo(150, true, true, true, kLime);
        QCOMPARE(w.psText(), QStringLiteral("PureSignal 3")); // !_mox branch
        QCOMPARE(w.psBackgroundColor(), kSeaGreen);
        // FB also takes the !_mox branch
        QCOMPARE(w.fbText(), QStringLiteral("Feedback"));
        QCOMPARE(w.fbBackgroundColor(), kSeaGreen);
    }

    void psInfo_withCorrectionsAppliedFalse_showsSeaGreen()
    {
        // Source: ucInfoBar.cs:861-865 [v2.10.3.13] — _bCorrectionsBeingApplied
        // false branch.  Even with calChanged=true and mox=true, the PS
        // label drops to SeaGreen "Pure Signal2" when the calcc engine is
        // not actively applying corrections (info[14] == 0).
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.psInfo(150, true, /*corrApplied=*/false, true, kLime);
        QCOMPARE(w.psText(), QStringLiteral("PureSignal 3"));
        QCOMPARE(w.psBackgroundColor(), kSeaGreen);
        // FB still shows numeric (calChanged=true && !hideFeedback)
        QCOMPARE(w.fbText(), QStringLiteral("150"));
    }

    void psInfo_setMoxFalse_clearsCalibrationAttemptsChanged()
    {
        // Source: ucInfoBar.cs:554-567 [v2.10.3.13] OnMoxChangeHandler.
        // When MOX flips off, setPSboolsToFalse() clears
        // _bCalibrationAttemptsChanged + _bCorrectionsBeingApplied +
        // _bFeedbackLevelOk.  After MOX flips back on, the FB label
        // correctly shows "Feedback" until a fresh psInfo() with
        // calChanged=true arrives — this is the path that prevents stale
        // numeric values from re-appearing post-MOX-cycle.
        PsaIndicatorWidget w(nullptr);
        w.setPsEnabled(true);
        w.setMox(true);
        w.psInfo(/*level=*/150, /*ok=*/true, /*corrApp=*/true,
                 /*calChanged=*/true, kLime);
        QCOMPARE(w.fbText(), QStringLiteral("150"));     // numeric

        w.setMox(false);
        // !_mox branch (ucInfoBar.cs:882-897 [v2.10.3.13]) always sets
        // lblFB.Text to "Feedback" regardless of stale state.
        QCOMPARE(w.fbText(), QStringLiteral("Feedback"));

        w.setMox(true);
        // Cal-attempts-changed was reset by setPSboolsToFalse on the MOX
        // edge.  No fresh psInfo() yet → numeric path is gated off.
        QCOMPARE(w.fbText(), QStringLiteral("Feedback"));
    }
};

QTEST_MAIN(TstPsaIndicator)
#include "tst_psa_indicator.moc"
