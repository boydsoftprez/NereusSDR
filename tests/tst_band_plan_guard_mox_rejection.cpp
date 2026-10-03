// =================================================================
// tests/tst_band_plan_guard_mox_rejection.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
// Phase 3M-1b Task K.2: MoxController::setMox(true) rejection path.
//
// Covers:
//   1. MoxCheckFn callback: CW mode → moxRejected("CW transmit is not available on this Core");
//      MOX state stays Rx.
//   2. AM mode → accepted (AM/SAM/DSB TX via WDSP ammod);
//      MOX state stays Rx.
//   3. LSB mode (allowed) → MOX engages normally; moxRejected NOT emitted.
//   4. No MoxCheckFn installed → setMox(true) succeeds (backwards-compat).
//   5. setMox(false) is never rejected — release path bypasses BandPlanGuard.
//   6. SPEC mode → moxRejected("This mode cannot transmit."); MOX stays Rx.
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
//   2026-09-22 : R-R3-36 Task 7 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. The RadioModel band-plan case keys
//                 from the radio mic; new cases pin the pre-check order
//                 (remote, band plan, PC microphone).
//   2026-09-28 : Addendum G-42 by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code. Extended is the Core's
//                 ExtendedTransmit setting; the old ExtendedTxAllowed is
//                 ignored (both keys restored by a scope guard). Item 4:
//                 band plan refusals in operator words.
//   2026-09-29 : Different-band transmit by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code. The Core's
//                 PreventTxOnDifferentBandToRx compares the transmitting
//                 slice with the same device's other slices.
//   2026-09-29 : Different-band transmit matches Thetis (JJ's ruling) by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code: only a transmitting slice that is not its device's
//                 active slice is compared, with the active slice.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QScopeGuard>
#include <QSignalSpy>

#include "core/MoxController.h"
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

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

    // ── 1. CW mode + setMox(true) → moxRejected("CW transmit is not available on this Core") ─

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
                 QStringLiteral("CW transmit is not available on this Core"));
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
                 QStringLiteral("CW transmit is not available on this Core"));
        QVERIFY(!ctrl.isMox());
    }

    // ── 2. AM mode → accepted (AM/SAM/DSB TX via WDSP ammod) ─────────────────

    void am_setMox_isAccepted()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck(makeCheckFn(DSPMode::AM));

        QSignalSpy rejectedSpy(&ctrl, &MoxController::moxRejected);

        ctrl.setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 0);
        QVERIFY(ctrl.isMox());
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
                 QStringLiteral("FM transmit is not available on this Core"));
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

    // ── 6. SPEC mode → moxRejected("This mode cannot transmit.") ──────────────

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
                 QStringLiteral("This mode cannot transmit."));
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
        // This case is about the band plan: key from the radio mic so the
        // R-R3-36 PC-microphone admission (capture is Closed here) stays
        // out of the way.
        model.transmitModel().setMicSource(MicSource::Radio);

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
                 QStringLiteral("4.500000 MHz with the transmit filter from 100 to 2900 Hz "
                                "reaches outside the transmit bands for your region "
                                "(United States)."));
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

    // ── R-R3-36: PC-microphone admission follows the band plan ─────────────
    //
    // The pre-check keeps its order: remote refusal, then band plan, then
    // the PC microphone. With PC mic selected and capture not Ready, an
    // out-of-band request is refused for the band plan, an in-band one for
    // the microphone, and neither advances the state machine.
    void radioModelMoxCheckRefusesPcMicAfterTheBandPlan()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
        QVERIFY(model.pcCaptureRequired());

        const int aId = model.addSlice();
        SliceModel* const a = model.sliceById(aId);
        QVERIFY(a);
        a->setDspMode(DSPMode::USB);
        a->setFrequency(4'500'000.0);
        QVERIFY(model.setActiveSliceById(aId));

        QSignalSpy rejectedSpy(model.moxController(), &MoxController::moxRejected);
        QSignalSpy txAboutToBeginSpy(model.moxController(), &MoxController::txAboutToBegin);
        QSignalSpy hardwareFlippedSpy(model.moxController(), &MoxController::hardwareFlipped);
        QSignalSpy stateChangedSpy(model.moxController(), &MoxController::stateChanged);

        model.moxController()->setMox(true);
        QCoreApplication::processEvents();
        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("4.500000 MHz with the transmit filter from 100 to 2900 Hz "
                                "reaches outside the transmit bands for your region "
                                "(United States)."));

        a->setFrequency(14'200'000.0);
        model.moxController()->setMox(true);
        QCoreApplication::processEvents();
        QCOMPARE(rejectedSpy.count(), 2);
        QCOMPARE(rejectedSpy.at(1).at(0).toString(),
                 QStringLiteral("Microphone is not ready. Check Audio settings and retry."));

        QVERIFY(!model.moxController()->isMox());
        QCOMPARE(model.moxController()->state(), MoxState::Rx);
        QCOMPARE(txAboutToBeginSpy.count(), 0);
        QCOMPARE(hardwareFlippedSpy.count(), 0);
        QCOMPARE(stateChangedSpy.count(), 0);
    }

    void radioModelChecksTransmitFilterEdges_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<double>("carrier");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("usb-upper-crossing") << int(DSPMode::USB) << 14349000.0 << false;
        QTest::newRow("usb-upper-inside") << int(DSPMode::USB) << 14347000.0 << true;
        QTest::newRow("lsb-lower-crossing") << int(DSPMode::LSB) << 14001000.0 << false;
        QTest::newRow("lsb-lower-inside") << int(DSPMode::LSB) << 14003000.0 << true;
        QTest::newRow("am-upper-crossing") << int(DSPMode::AM) << 14349000.0 << false;
        QTest::newRow("am-lower-crossing") << int(DSPMode::AM) << 14001000.0 << false;
        QTest::newRow("rade-upper-crossing") << int(DSPMode::RADE_U) << 14349000.0 << false;
        QTest::newRow("rade-lower-crossing") << int(DSPMode::RADE_L) << 14001000.0 << false;
    }

    void radioModelChecksTransmitFilterEdges()
    {
        QFETCH(int, mode);
        QFETCH(double, carrier);
        QFETCH(bool, allowed);
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();
        model.transmitModel().setMicSource(MicSource::Radio);
        SliceModel* slice = model.sliceById(model.addSlice());
        QVERIFY(slice);
        slice->setDspMode(static_cast<DSPMode>(mode));
        slice->setFrequency(carrier);
        model.transmitModel().setFilterLow(100);
        model.transmitModel().setFilterHigh(2900);
        QSignalSpy rejected(model.moxController(), &MoxController::moxRejected);
        model.moxController()->setMox(true);
        QCoreApplication::processEvents();
        QCOMPARE(model.moxController()->isMox(), allowed);
        QCOMPARE(rejected.size(), allowed ? 0 : 1);
        model.moxController()->setMox(false);
        QCoreApplication::processEvents();
    }

    // Addendum G-42 (JJ's ruling 2026-09-28): Extended is one Core
    // setting, ExtendedTransmit, read at every key. Thetis's
    // CheckValidTXFreq returns true while it is on (console.cs:6780
    // [v2.10.3.15]), past the band edges, the filter edges and the US 60 m
    // mode rule. An old saved ExtendedTxAllowed never turns it on, and
    // only exactly "True" does.
    void extendedTransmitIsTheCoresSettingAndTheOldKeyIsIgnored_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<double>("carrier");
        QTest::newRow("usb-carrier-above-20m") << int(DSPMode::USB) << 14360000.0;
        QTest::newRow("usb-filter-edge-above-20m") << int(DSPMode::USB) << 14349000.0;
        QTest::newRow("lsb-filter-edge-below-20m") << int(DSPMode::LSB) << 14001000.0;
        QTest::newRow("am-on-us-60m") << int(DSPMode::AM) << 5357000.0;
    }

    void extendedTransmitIsTheCoresSettingAndTheOldKeyIsIgnored()
    {
        QFETCH(int, mode);
        QFETCH(double, carrier);
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();
        model.transmitModel().setMicSource(MicSource::Radio);
        SliceModel* slice = model.sliceById(model.addSlice());
        QVERIFY(slice);
        slice->setDspMode(static_cast<DSPMode>(mode));
        slice->setFrequency(carrier);
        model.transmitModel().setFilterLow(100);
        model.transmitModel().setFilterHigh(2900);

        const auto keyed = [&model]() {
            model.moxController()->setMox(true);
            QCoreApplication::processEvents();
            const bool on = model.moxController()->isMox();
            model.moxController()->setMox(false);
            QCoreApplication::processEvents();
            return on;
        };
        auto& settings = AppSettings::instance();
        // Whatever the two keys held before this case, they hold again after
        // it, pass or fail.
        const QStringList keys{QStringLiteral("ExtendedTxAllowed"),
                               QStringLiteral("ExtendedTransmit")};
        QHash<QString, QVariant> before;
        for (const QString& key : keys) {
            if (settings.contains(key)) {
                before.insert(key, settings.value(key));
            }
        }
        const auto restore = qScopeGuard([&settings, &keys, &before] {
            for (const QString& key : keys) {
                if (before.contains(key)) {
                    settings.setValue(key, before.value(key));
                } else {
                    settings.remove(key);
                }
            }
        });
        for (const QString& key : keys) {
            settings.remove(key);
        }
        // Off by default.
        QVERIFY(!keyed());
        // The old per-computer key is ignored.
        settings.setValue(QStringLiteral("ExtendedTxAllowed"), QStringLiteral("True"));
        QVERIFY(!keyed());
        settings.setValue(QStringLiteral("ExtendedTransmit"), QStringLiteral("true"));
        QVERIFY(!keyed());
        settings.setValue(QStringLiteral("ExtendedTransmit"), QStringLiteral("True"));
        QVERIFY(keyed());
        settings.setValue(QStringLiteral("ExtendedTransmit"), QStringLiteral("False"));
        QVERIFY(!keyed());
        settings.remove(QStringLiteral("ExtendedTransmit"));
        QVERIFY(!keyed());
    }

    // JJ's rulings (2026-09-29): Thetis's "Prevent TX'ing on a different
    // band to the RX band" (_preventTXonDifferentBandToRXband, default off,
    // console.cs:29451-29465 [v2.10.3.15]) is one Core setting,
    // PreventTxOnDifferentBandToRx, and it matches Thetis: Thetis refuses
    // only when it transmits on VFO B (split) and VFO B's band differs
    // from the RX band. NereusSDR has no split; the counterpart is a
    // transmitting slice that is not its device's active (listening) slice
    // while on a different band from it. A second slice parked on another
    // band never blocks a key on the active slice. Only exactly "True"
    // turns it on.
    void preventDifferentBandComparesWithTheDevicesActiveSlice()
    {
        auto& settings = AppSettings::instance();
        const QString key = QStringLiteral("PreventTxOnDifferentBandToRx");
        const auto restore = qScopeGuard([&settings, &key] { settings.remove(key); });
        settings.remove(key);

        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();
        model.transmitModel().setMicSource(MicSource::Radio);
        const int aId = model.addSlice();
        const int bId = model.addSlice();
        SliceModel* const a = model.sliceById(aId);
        SliceModel* const b = model.sliceById(bId);
        QVERIFY(a && b);
        a->setDspMode(DSPMode::USB);
        a->setFrequency(14'200'000.0);
        b->setDspMode(DSPMode::LSB);
        b->setFrequency(7'150'000.0);
        QVERIFY(model.setActiveSliceById(aId));
        SliceOwnership* const owners = model.sliceOwnership();
        TxSliceArbiter* const arbiter = model.txSliceArbiter();
        MoxController* const mox = model.moxController();
        // The flag moves at once only when the walk back to receive is done.
        const auto transmitOn = [arbiter, mox](int sliceId) {
            return QTest::qWaitFor([mox] { return mox->state() == MoxState::Rx; }, 2000)
                && arbiter->requestHandoff(sliceId) && arbiter->txBoundSliceId() == sliceId;
        };
        QVERIFY(transmitOn(aId));

        QSignalSpy rejected(model.moxController(), &MoxController::moxRejected);
        const auto keyed = [&model]() {
            model.moxController()->setMox(true);
            QCoreApplication::processEvents();
            const bool on = model.moxController()->isMox();
            model.moxController()->setMox(false);
            QCoreApplication::processEvents();
            return on;
        };
        const QString refusal = QStringLiteral(
            "Transmit would be on 40 m while another slice you have open is on 20 m, "
            "and Setup is set to prevent transmitting on a different band.");

        // Off (Thetis's default): never refuses, not even the parked slice.
        QVERIFY(keyed());
        QVERIFY(transmitOn(bId));
        QVERIFY(keyed());
        settings.setValue(key, QStringLiteral("true"));
        QVERIFY(keyed());
        settings.setValue(key, QStringLiteral("False"));
        QVERIFY(keyed());
        QCOMPARE(rejected.size(), 0);

        // On. These slices have no owner, as on a desktop with no devices:
        // the station window operates them, so they are one device.
        settings.setValue(key, QStringLiteral("True"));
        QVERIFY(owners->mark(aId).owner.isEmpty());
        QVERIFY(owners->mark(bId).owner.isEmpty());

        // Slice A 20 m active, slice B parked on 40 m: A transmits.
        QVERIFY(transmitOn(aId));
        QVERIFY(keyed());
        QCOMPARE(rejected.size(), 0);

        // B transmits while A is the active slice on 20 m: refused,
        // naming both bands.
        QVERIFY(transmitOn(bId));
        QVERIFY(!keyed());
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), refusal);

        // B on A's band: keys.
        b->setFrequency(14'250'000.0);
        QVERIFY(keyed());
        b->setFrequency(7'150'000.0);

        // B made active: it transmits where it listens, A parked on 20 m.
        QVERIFY(model.setActiveSliceById(bId));
        QVERIFY(keyed());
        QCOMPARE(rejected.size(), 1);
        QVERIFY(model.setActiveSliceById(aId));

        // The station's own slices and ownerless ones are one device.
        owners->setOwner(bId, SliceOwnership::stationDevice());
        QVERIFY(!keyed());
        QCOMPARE(rejected.size(), 2);
        owners->setOwner(aId, SliceOwnership::stationDevice());
        owners->setActive(SliceOwnership::stationDevice(), aId);
        QVERIFY(!keyed());
        QCOMPARE(rejected.size(), 3);

        // Another device's active slice does not count: the phone's A on
        // 20 m, the station's B transmitting where it listens.
        owners->setOwner(aId, QByteArrayLiteral("phone"));
        owners->setActive(QByteArrayLiteral("phone"), aId);
        QVERIFY(keyed());
        QCOMPARE(rejected.size(), 3);

        // Both the phone's: its active A on 20 m refuses B, and A itself
        // transmits.
        owners->setOwner(bId, QByteArrayLiteral("phone"));
        owners->setActive(QByteArrayLiteral("phone"), aId);
        QVERIFY(!keyed());
        QCOMPARE(rejected.size(), 4);
        QCOMPARE(rejected.at(3).at(0).toString(), refusal);
        QVERIFY(transmitOn(aId));
        QVERIFY(keyed());
        QVERIFY(transmitOn(bId));

        // Slices held for the absent phone are the phone's (the mark's
        // subject), not the station device's: the station runs them, and
        // its choice among them is the phone's active slice.
        owners->hold(aId, QByteArrayLiteral("phone"));
        owners->hold(bId, QByteArrayLiteral("phone"));
        owners->setActive(SliceOwnership::stationDevice(), aId);
        QVERIFY(!keyed());
        QCOMPARE(rejected.size(), 5);
        owners->setActive(SliceOwnership::stationDevice(), bId);
        QVERIFY(keyed());
        // A held for the phone does not stand for the station's own B.
        owners->setActive(SliceOwnership::stationDevice(), aId);
        owners->setOwner(bId, SliceOwnership::stationDevice());
        QVERIFY(keyed());
        QCOMPARE(rejected.size(), 5);

        // Off again: never refuses.
        owners->hold(bId, QByteArrayLiteral("phone"));
        QVERIFY(!keyed());
        QCOMPARE(rejected.size(), 6);
        settings.remove(key);
        QVERIFY(keyed());
        QCOMPARE(rejected.size(), 6);
    }

    // Thetis checks the different band first, before the US 60 m mode rule
    // and CheckValidTXFreq (console.cs:29451, :29467, :29486 [v2.10.3.15]);
    // the mode allow-list is NereusSDR's and stays ahead of it.
    void preventDifferentBandRefusesBeforeTheBandEdges()
    {
        auto& settings = AppSettings::instance();
        const QString key = QStringLiteral("PreventTxOnDifferentBandToRx");
        const auto restore = qScopeGuard([&settings, &key] { settings.remove(key); });
        settings.setValue(key, QStringLiteral("True"));

        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();
        model.transmitModel().setMicSource(MicSource::Radio);
        model.transmitModel().setFilterLow(100);
        model.transmitModel().setFilterHigh(2900);
        const int aId = model.addSlice();
        const int bId = model.addSlice();
        SliceModel* const a = model.sliceById(aId);
        SliceModel* const b = model.sliceById(bId);
        QVERIFY(a && b);
        b->setDspMode(DSPMode::LSB);
        b->setFrequency(7'150'000.0);
        // A transmits while B, the active slice, listens on 40 m.
        QVERIFY(model.txSliceArbiter()->requestHandoff(aId));
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), aId);
        QVERIFY(model.setActiveSliceById(bId));
        QSignalSpy rejected(model.moxController(), &MoxController::moxRejected);
        const auto refusal = [&]() {
            model.moxController()->setMox(true);
            QCoreApplication::processEvents();
            model.moxController()->setMox(false);
            QCoreApplication::processEvents();
            return rejected.isEmpty() ? QString() : rejected.takeLast().at(0).toString();
        };

        // A filter edge past the top of 20 m.
        a->setDspMode(DSPMode::USB);
        a->setFrequency(14'349'000.0);
        QCOMPARE(refusal(), QStringLiteral(
            "Transmit would be on 20 m while another slice you have open is on 40 m, "
            "and Setup is set to prevent transmitting on a different band."));
        // AM on US 60 m.
        a->setDspMode(DSPMode::AM);
        a->setFrequency(5'357'000.0);
        QCOMPARE(refusal(), QStringLiteral(
            "Transmit would be on 60 m while another slice you have open is on 40 m, "
            "and Setup is set to prevent transmitting on a different band."));
        // A mode that cannot transmit is still named first.
        a->setDspMode(DSPMode::FM);
        a->setFrequency(14'200'000.0);
        QCOMPARE(refusal(), QStringLiteral("FM transmit is not available on this Core"));
    }

    void invalidStoredRegionCannotWrapIntoAnAllowedRegion_data()
    {
        QTest::addColumn<QString>("region");
        QTest::newRow("not-an-integer") << QStringLiteral("invalid");
        QTest::newRow("fraction") << QStringLiteral("8.5");
        QTest::newRow("positive-wrap") << QStringLiteral("264");
        QTest::newRow("negative-wrap") << QStringLiteral("-248");
        QTest::newRow("empty") << QString();
    }

    void invalidStoredRegionCannotWrapIntoAnAllowedRegion()
    {
        QFETCH(QString, region);
        AppSettings::instance().setValue(QStringLiteral("BandPlanRegion"), region);
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.installBandPlanMoxCheckForTest();
        model.transmitModel().setMicSource(MicSource::Radio);
        SliceModel* slice = model.sliceById(model.addSlice());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000.0);
        QSignalSpy rejected(model.moxController(), &MoxController::moxRejected);
        model.moxController()->setMox(true);
        QCoreApplication::processEvents();
        QVERIFY(!model.moxController()->isMox());
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), QStringLiteral("The transmit region setting is invalid."));
    }

    // The remote refusal is unchanged and still comes first.
    void remoteRefusalPrecedesPcMicAdmission()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);

        QSignalSpy rejectedSpy(model.moxController(), &MoxController::moxRejected);
        model.moxController()->setMox(true);
        QCoreApplication::processEvents();

        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.at(0).at(0).toString(),
                 QStringLiteral("Remote transmit controls are not available "
                                "from this Core."));
        QVERIFY(!model.moxController()->isMox());
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
        QCOMPARE(tip, QStringLiteral("CW transmit is not available on this Core"));
    }

    void tooltipForMode_cwu_returnsCwPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::CWU);
        QCOMPARE(tip, QStringLiteral("CW transmit is not available on this Core"));
    }

    void tooltipForMode_am_returnsManualMox()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::AM);
        QCOMPARE(tip, QStringLiteral("Manual transmit (MOX)"));
    }

    void tooltipForMode_fm_returnsAudioPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::FM);
        QCOMPARE(tip, QStringLiteral("FM transmit is not available on this Core"));
    }

    void tooltipForMode_sam_returnsManualMox()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::SAM);
        QCOMPARE(tip, QStringLiteral("Manual transmit (MOX)"));
    }

    void tooltipForMode_dsb_returnsManualMox()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::DSB);
        QCOMPARE(tip, QStringLiteral("Manual transmit (MOX)"));
    }

    void tooltipForMode_drm_returnsAudioPhase()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::DRM);
        QCOMPARE(tip, QStringLiteral("DRM transmit is not available on this Core"));
    }

    void tooltipForMode_spec_returnsNotSupported()
    {
        const QString tip = TxApplet::tooltipForMode(DSPMode::SPEC);
        QCOMPARE(tip, QStringLiteral("This mode cannot transmit."));
    }
};

QTEST_GUILESS_MAIN(TestBandPlanGuardMoxRejection)
#include "tst_band_plan_guard_mox_rejection.moc"
