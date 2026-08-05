// =================================================================
// tests/tst_band_plan_guard_mox_rejection.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
// Phase 3M-1b Task K.2: MoxController::setMox(true) rejection path.
//
// Covers:
//   1. MoxCheckFn callback: CW mode → moxRejected("CW TX coming in Phase 3M-2");
//      MOX state stays Rx.
//   2. AM mode → moxRejected("AM/FM TX coming in Phase 3M-3 (audio modes)");
//      MOX state stays Rx.
//   3. LSB mode (allowed) → MOX engages normally; moxRejected NOT emitted.
//   4. No MoxCheckFn installed → setMox(true) succeeds (backwards-compat).
//   5. setMox(false) is never rejected — release path bypasses BandPlanGuard.
//   6. SPEC mode → moxRejected("Mode not supported for TX"); MOX stays Rx.
//   7. Rejection: no state advance, no phase signals (txAboutToBegin not emitted).
//   8. After rejection, setMox(true) with CW can be re-attempted; still rejects.
//
// Additionally tests TxApplet::tooltipForMode static helper:
//   9.  USB → normal tooltip.
//  10.  LSB → normal tooltip.
//  11.  DIGL → normal tooltip.
//  12.  DIGU → normal tooltip.
//  13.  CWL → CW deferred tooltip.
//  14.  CWU → CW deferred tooltip.
//  15.  AM  → audio modes deferred tooltip.
//  16.  FM  → audio modes deferred tooltip.
//  17.  SAM → audio modes deferred tooltip.
//  18.  DSB → audio modes deferred tooltip.
//  19.  DRM → audio modes deferred tooltip.
//  20.  SPEC → not-supported tooltip.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-28 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//                 Task: Phase 3M-1b Task K.2 — MOX rejection signal +
//                 status-bar toast + TxApplet tooltip override. Closes Phase K.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/MoxController.h"
#include "core/AppSettings.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using namespace NereusSDR::safety;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr Region   kRegion  = Region::UnitedStates;
static constexpr std::int64_t kFreqHz = 14'200'000; // US 20m, well in-band
static constexpr Band     kBand20m = Band::Band20m;

/// Build a MoxCheckFn that always returns {ok=false, reason} for the given mode.
/// Simulates RadioModel's lambda: calls checkMoxAllowed with the given mode.
static MoxController::MoxCheckFn makeCheckFn(DSPMode mode)
{
    return [mode]() -> BandPlanGuard::MoxCheckResult {
        BandPlanGuard guard;
        return guard.checkMoxAllowed(
            kRegion, kFreqHz, mode,
            kBand20m, kBand20m,
            /*preventDifferentBand=*/false,
            /*extended=*/false);
    };
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestBandPlanGuardMoxRejection : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(
            QStringLiteral("BandPlanRegion"),
            QString::number(static_cast<int>(Region::UnitedStates)));
    }

    void cleanup()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(
            QStringLiteral("BandPlanRegion"),
            QString::number(static_cast<int>(Region::UnitedStates)));
    }

    // ── 1. CW mode + setMox(true) → moxRejected("CW TX coming in Phase 3M-2") ─

    void cwl_setMox_emitsMoxRejected()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::CWL));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);
        QSignalSpy stateChangedSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("CW TX coming in Phase 3M-2"));
        // MOX state must NOT have advanced.
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.state(), MoxState::Rx);
        QCOMPARE(stateChangedSpy.count(), 0);
    }

    void cwu_setMox_emitsMoxRejected()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::CWU));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("CW TX coming in Phase 3M-2"));
        QVERIFY(!ctrl.isMox());
    }

    // ── 2. AM mode → moxRejected("AM/FM TX coming in Phase 3M-3 (audio modes)") ─

    void am_setMox_emitsMoxRejected()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::AM));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
        QVERIFY(!ctrl.isMox());
    }

    void fm_setMox_emitsMoxRejected()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::FM));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
        QVERIFY(!ctrl.isMox());
    }

    // ── 3. LSB mode (allowed) → MOX engages; moxRejected NOT emitted ───────────

    void lsb_setMox_engagesNormally()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::LSB));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);
        QSignalSpy moxChangedSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 0);
        QCOMPARE(moxChangedSpy.count(), 1);
        QVERIFY(ctrl.isMox());
    }

    void usb_setMox_engagesNormally()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::USB));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 0);
        QVERIFY(ctrl.isMox());
    }

    // ── 4. No MoxCheckFn installed → setMox(true) succeeds (backwards-compat) ──

    void noCheckFn_setMox_succeedsByDefault()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        // No setMoxCheck() call — m_moxCheck is empty/null.

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);
        QSignalSpy moxChangedSpy(&ctrl, &MoxController::moxStateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 0);
        QCOMPARE(moxChangedSpy.count(), 1);
        QVERIFY(ctrl.isMox());
    }

    // ── 5. setMox(false) never rejected — release path bypasses BandPlanGuard ──

    void setMoxFalse_neverRejected()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);

        // Engage MOX without a check (no check fn installed yet).
        ctrl.setMox(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isMox());

        // Now install a CW check fn (which would reject setMox(true) if re-engaged).
        ctrl.setMoxCheck(makeCheckFn(DSPMode::CWL));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        // setMox(false) must succeed regardless of the installed check fn.
        ctrl.setMox(false);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 0);
        QVERIFY(!ctrl.isMox());
    }

    // ── 6. SPEC mode → moxRejected("Mode not supported for TX") ───────────────

    void spec_setMox_emitsMoxRejected()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::SPEC));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("Mode not supported for TX"));
        QVERIFY(!ctrl.isMox());
    }

    // ── 7. Rejection: no phase signals (txAboutToBegin NOT emitted) ─────────────

    void rejection_doesNotEmitPhaseSignals()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::CWL));

        QSignalSpy txAboutToBeginSpy(&ctrl, &MoxController::txAboutToBegin);
        QSignalSpy hardwareFlippedSpy(&ctrl, &MoxController::hardwareFlipped);
        QSignalSpy stateChangedSpy(&ctrl,   &MoxController::stateChanged);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(txAboutToBeginSpy.count(), 0);
        QCOMPARE(hardwareFlippedSpy.count(), 0);
        QCOMPARE(stateChangedSpy.count(), 0);
    }

    // ── 8. After rejection, re-attempting still rejects ─────────────────────────

    void rejection_isRepeatable()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::CWL));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        QCOMPARE(rejectedSpy.count(), 1);

        ctrl.setMox(true);
        QCoreApplication::processEvents();
        QCOMPARE(rejectedSpy.count(), 2);  // second attempt also rejected

        QVERIFY(!ctrl.isMox());
    }

    void radioModelMoxCheckUsesTheTxBoundSliceInBothLegalityDirections()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();

        const int aId = model.addSlice();
        SliceModel* const a = model.sliceById(aId);
        QVERIFY(a);
        a->setDspMode(DSPMode::USB);
        a->setFrequency(14'200'000.0);

        model.addSlice();
        const int cId = model.addSlice();
        SliceModel* const c = model.sliceById(cId);
        QVERIFY(c);
        c->setDspMode(DSPMode::USB);
        c->setFrequency(4'500'000.0);

        QVERIFY(model.setActiveSliceById(aId));
        QVERIFY(model.txSliceArbiter()->requestHandoff(cId));

        QSignalSpy rejectedSpy(model.moxController(), &MoxController::moxRejected);
        model.moxController()->setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("Frequency outside TX-allowed range"));
        QVERIFY(!model.moxController()->isMox());

        a->setFrequency(4'500'000.0);
        c->setFrequency(7'100'000.0);
        rejectedSpy.clear();

        model.moxController()->setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 0);
        QVERIFY(model.moxController()->isMox());
        model.moxController()->setMox(false);
        QCoreApplication::processEvents();
    }

    // ── 9-20: TxApplet::tooltipForMode static helper ────────────────────────────
    // These tests exercise the helper directly without constructing a full
    // TxApplet (which requires a RadioModel + Qt widgets).

    void tooltipForMode_usb_returnsNormal()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::USB);
        QCOMPARE(tip, QStringLiteral("Manual transmit (MOX)"));
    }

    void tooltipForMode_lsb_returnsNormal()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::LSB);
        QCOMPARE(tip, QStringLiteral("Manual transmit (MOX)"));
    }

    void tooltipForMode_digl_returnsNormal()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::DIGL);
        QCOMPARE(tip, QStringLiteral("Manual transmit (MOX)"));
    }

    void tooltipForMode_digu_returnsNormal()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::DIGU);
        QCOMPARE(tip, QStringLiteral("Manual transmit (MOX)"));
    }

    void tooltipForMode_cwl_returnsCwPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::CWL);
        QCOMPARE(tip, QStringLiteral("CW TX coming in Phase 3M-2"));
    }

    void tooltipForMode_cwu_returnsCwPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::CWU);
        QCOMPARE(tip, QStringLiteral("CW TX coming in Phase 3M-2"));
    }

    void tooltipForMode_am_returnsAudioPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::AM);
        QCOMPARE(tip, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
    }

    void tooltipForMode_fm_returnsAudioPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::FM);
        QCOMPARE(tip, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
    }

    void tooltipForMode_sam_returnsAudioPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::SAM);
        QCOMPARE(tip, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
    }

    void tooltipForMode_dsb_returnsAudioPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::DSB);
        QCOMPARE(tip, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
    }

    void tooltipForMode_drm_returnsAudioPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::DRM);
        QCOMPARE(tip, QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)"));
    }

    void tooltipForMode_spec_returnsNotSupported()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::SPEC);
        QCOMPARE(tip, QStringLiteral("Mode not supported for TX"));
    }
};

QTEST_GUILESS_MAIN(TestBandPlanGuardMoxRejection)
#include "tst_band_plan_guard_mox_rejection.moc"
