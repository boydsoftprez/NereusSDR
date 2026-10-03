// no-port-check: NereusSDR-original unit-test file. No Thetis logic is
// ported here; this exercises NereusSDR's own SliceMeterPump wiring
// (remote-daemon R2 Task 12). The WDSP meter reads it drives ARE Thetis
// ports (see SliceMeterPump.cpp's own citations), but nothing in this file
// re-derives that logic independently -- it calls the same production
// accessors (RxChannel::getMeter, WdspEngine::getMaxBinDbm) SliceMeterPump
// itself calls, so a test failure means the wiring is wrong, not that this
// file disagrees with Thetis about a meter formula.
// =================================================================
// tests/tst_slice_meter_pump.cpp  (NereusSDR)
// =================================================================
//
// Remote-daemon R2 Task 12 -- the per-slice S-meter gets a model home.
//
// SliceMeterPump replaces MeterPoller::pollSliceSMeters() (GUI-only, so a
// headless nereusd could never produce a per-slice S-meter reading for the
// mirror to carry) with a core-side QTimer that writes directly into each
// live slice's SliceModel::signalStrengthDbm. See SliceMeterPump.h's class
// comment for the full design.
//
// Test groups:
//   1. SliceModel::signalStrengthDbm's shape: no WRITE, has a NOTIFY,
//      defaults to -140.0, emits once per distinct value (bare SliceModel,
//      no RadioModel needed).
//   2. poll() with no reading to give: a link that is not Connected, or a
//      slice with no WDSP channel, gets the -400 dBm no-reading value on
//      all three readings (R-R3-13), including LinkLost with a live
//      channel and its recovery to Connected; TX (RadioStatus::
//      isTransmitting) means poll() touches nothing at all, even a slice
//      that already holds a real reading.
//   3. poll() against a REAL connected RxChannel (fakes/ConnectableRadioModel),
//      proving the source selector actually reaches WDSP: SignalAverage
//      (the default with no selector wired) and SignalPeak both match an
//      independently-taken read of the SAME live meter; MaxBin, with no
//      detector ever configured, passes the WdspEngine::getMaxBinDbm
//      -400.0 "not active" sentinel straight through -- deterministic
//      without needing a live FFT displayed at all.
//   4. Construction is gated on RadioModel::Role -- the obligation Task 5's
//      brief could not discharge (SliceMeterPump did not exist yet) and
//      reassigned here. See tst_remote_role_inert.cpp's own header comment
//      for that history.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-06 -- New test file for remote-daemon R2 Task 12. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-23 -- R-R3-13: no-reading cases for a link that is not
//                 Connected and a slice with no channel. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QPair>

#include <memory>

#include "core/ConnectionState.h"
#include "core/RadioStatus.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/meters/SliceMeterPump.h"
#include "fakes/ConnectableRadioModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
// R-R3-39: the pump reads the receive lane's meter cache, and its reads ask
// the lane for a fresh one; a slice shows no reading until the lane has read
// its channel. Poll, let the lane read, then poll again, as the pump's timer
// would.
void pollSettled(RadioModel& model, SliceMeterPump* pump)
{
    pump->poll();
    QVERIFY(model.waitForReceiveLaneForTest());
    pump->poll();
}
} // namespace
using NereusSDR::Test::ConnectableRadioModel;

class TestSliceMeterPump : public QObject {
    Q_OBJECT

private slots:

    // ── Group 1: SliceModel's S-meter telemetry shape ──────────────────────

    // Step 1: "not writable, has a notify, starts at -140.0". Pure
    // meta-object introspection plus a freshly-constructed default -- no
    // RadioModel, no WdspEngine, nothing that could touch a real radio.
    void signalReadingsAreReadOnlyWithNotifyAndDefaultToMinus140()
    {
        SliceModel slice(0);

        const QMetaObject* mo = slice.metaObject();
        const QList<QPair<QByteArray, QByteArray>> readings = {
            {"signalStrengthDbm", "signalStrengthDbmChanged"},
            {"signalPeakDbm", "signalPeakDbmChanged"},
            {"signalAverageDbm", "signalAverageDbmChanged"},
        };
        for (const auto& [name, notify] : readings) {
            const int idx = mo->indexOfProperty(name.constData());
            QVERIFY2(idx >= 0, qPrintable(name + " must be a declared Q_PROPERTY"));
            const QMetaProperty prop = mo->property(idx);
            QVERIFY2(!prop.isWritable(),
                     qPrintable(name + " must carry no WRITE accessor"));
            QVERIFY2(prop.hasNotifySignal(),
                     qPrintable(name + " must have a NOTIFY for mirror deltas"));
            QCOMPARE(prop.notifySignal().name(), notify);
        }

        QCOMPARE(slice.signalStrengthDbm(), -140.0);
        QCOMPARE(slice.signalPeakDbm(), -140.0);
        QCOMPARE(slice.signalAverageDbm(), -140.0);
    }

    // "Emits once per distinct value." A property of setSignalStrengthDbm's
    // own equality guard, tested directly against the setter rather than
    // through a live poll -- a live WDSP meter reading two ticks apart is
    // not guaranteed identical even under silence, which would make this
    // assertion flaky for a reason that has nothing to do with the emit
    // guard under test.
    void setSignalStrengthDbmEmitsOncePerDistinctValue()
    {
        SliceModel slice(0);
        QSignalSpy spy(&slice, &SliceModel::signalStrengthDbmChanged);

        slice.setSignalStrengthDbm(-73.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.constLast().at(0).toDouble(), -73.0);

        // Same value again: no second emit.
        slice.setSignalStrengthDbm(-73.0);
        QCOMPARE(spy.count(), 1);

        // A distinct value: exactly one more emit.
        slice.setSignalStrengthDbm(-91.5);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.constLast().at(0).toDouble(), -91.5);
    }

    void sourceReadingsEmitOncePerDistinctValueAndAcceptMirrorState()
    {
        SliceModel slice(0);
        QSignalSpy peakSpy(&slice, &SliceModel::signalPeakDbmChanged);
        QSignalSpy averageSpy(&slice, &SliceModel::signalAverageDbmChanged);

        slice.setSignalPeakDbm(-73.0);
        slice.setSignalPeakDbm(-73.0);
        QCOMPARE(peakSpy.count(), 1);
        QCOMPARE(slice.applyMirroredValue("signalPeakDbm", QVariant(-71.5)), QString());
        QCOMPARE(peakSpy.count(), 2);
        QCOMPARE(slice.signalPeakDbm(), -71.5);

        slice.setSignalAverageDbm(-91.0);
        slice.setSignalAverageDbm(-91.0);
        QCOMPARE(averageSpy.count(), 1);
        QCOMPARE(slice.applyMirroredValue("signalAverageDbm", QVariant(-89.5)), QString());
        QCOMPARE(averageSpy.count(), 2);
        QCOMPARE(slice.signalAverageDbm(), -89.5);
    }

    // ── Group 2: poll() against an unconnected RadioModel ───────────────────

    // R-R3-13 (decided behaviour change, 2026-09-23): this test used to
    // assert that poll() LEFT a channel-less slice at its constructed
    // -140.0 default. That default is a floor number the flag bar draws as
    // "-140 dBm", a reading that does not exist. The plan settles that a
    // local flag with no reading shows "-- dBm", so poll() now writes the
    // -400 dBm no-reading value (SliceMeterPump::kNoReadingDbm) to all
    // three readings. A bare, never-connected Role::Local model is not
    // Connected, so this exercises the link-down gate; the Connected
    // no-channel branch has its own case below. Not a weakened test: the
    // exact value is still pinned, only the decided value changed.
    void pollWritesNoReadingWhenNotConnectedAndNoWdspChannelExists()
    {
        RadioModel model{RadioModel::Role::Local};
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        const int sliceId = model.addSlice();
        QVERIFY(sliceId >= 0);
        SliceModel* slice = model.sliceById(sliceId);
        QVERIFY(slice != nullptr);
        QVERIFY(model.wdspEngine()->rxChannel(slice->sliceIndex()) == nullptr);
        QVERIFY(model.connectionState() != ConnectionState::Connected);

        pollSettled(model, pump);

        QCOMPARE(slice->signalStrengthDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalPeakDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalAverageDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(SliceMeterPump::kNoReadingDbm, -400.0);
    }

    // R-R3-13: the no-channel branch on its own. The model reports
    // Connected (test seam) but the slice still has no WDSP channel, so the
    // link gate passes and the per-slice branch must write the no-reading
    // value itself rather than leave a seeded stale reading in place.
    void pollWritesNoReadingForAConnectedSliceWithNoWdspChannel()
    {
        RadioModel model{RadioModel::Role::Local};
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        const int sliceId = model.addSlice();
        SliceModel* slice = model.sliceById(sliceId);
        QVERIFY(slice != nullptr);
        QVERIFY(model.wdspEngine()->rxChannel(slice->sliceIndex()) == nullptr);

        slice->setSignalStrengthDbm(-73.0);
        slice->setSignalPeakDbm(-70.0);
        slice->setSignalAverageDbm(-75.0);

        model.setConnectionStateForTest(ConnectionState::Connected);
        pollSettled(model, pump);

        QCOMPARE(slice->signalStrengthDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalPeakDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalAverageDbm(), SliceMeterPump::kNoReadingDbm);
    }

    // R-R3-13: LinkLost with a live channel. A local LinkLost keeps the
    // RX channels alive, so without the link gate poll() would keep
    // publishing the channel's frozen (or floor) meter. The flag must read
    // no reading while the link is down, even while transmitting (the link
    // gate sits before the TX gate), and live readings must come back once
    // the link recovers to Connected.
    void pollWritesNoReadingOnLinkLostWithLiveChannelAndRecovers()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        RxChannel* ch = model.wdspEngine()->rxChannel(slice->sliceIndex());
        QVERIFY(ch != nullptr);

        // Connected first: a real reading lands.
        pollSettled(model, pump);
        QVERIFY(slice->signalAverageDbm() > SliceMeterPump::kNoReadingDbm);

        model.setConnectionStateForTest(ConnectionState::LinkLost);
        // The channel survives LinkLost; that is exactly why the gate is
        // needed.
        QVERIFY(model.wdspEngine()->rxChannel(slice->sliceIndex()) != nullptr);
        pollSettled(model, pump);

        QCOMPARE(slice->signalStrengthDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalPeakDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalAverageDbm(), SliceMeterPump::kNoReadingDbm);

        // Link down while keyed still clears: re-seed, key, poll.
        slice->setSignalStrengthDbm(-73.0);
        slice->setSignalPeakDbm(-70.0);
        slice->setSignalAverageDbm(-75.0);
        model.radioStatus().setTransmitting(true);
        pollSettled(model, pump);
        QCOMPARE(slice->signalStrengthDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalPeakDbm(), SliceMeterPump::kNoReadingDbm);
        QCOMPARE(slice->signalAverageDbm(), SliceMeterPump::kNoReadingDbm);
        model.radioStatus().setTransmitting(false);

        // Recovery: Connected again publishes the live channel's readings.
        model.setConnectionStateForTest(ConnectionState::Connected);
        pollSettled(model, pump);

        const double average =
            ch->getMeter(RxMeterType::SignalAvg) + model.rxMeterOffsetDb();
        QCOMPARE(slice->signalStrengthDbm(), average);
        QCOMPARE(slice->signalPeakDbm(),
                 ch->getMeter(RxMeterType::SignalPeak) + model.rxMeterOffsetDb());
        QCOMPARE(slice->signalAverageDbm(), average);
        QVERIFY(slice->signalAverageDbm() > SliceMeterPump::kNoReadingDbm);

        harness.reset();
    }

    // Step 4: "the pump stops while transmitting." RadioStatus::
    // isTransmitting() (not MeterPoller's m_inTx, which only ever sees MOX
    // asserted through MoxController) gates poll() before it looks at a
    // single slice.
    //
    // Fix round 1 (reviewer finding): the original version of this test
    // used an UNCONNECTED model, where wdspEngine()->rxChannel(id) is
    // already null. At the time poll()'s no-channel branch was a
    // `continue` that left the slice untouched regardless of the TX gate
    // (since R-R3-13 an unconnected model gets the no-reading value
    // instead; see the cases above), so that version passed
    // whether or not isTransmitting() was ever checked -- deleting the
    // gate, or moving it below the per-slice loop, would not have failed
    // it. This version uses a REAL connected RxChannel
    // (ConnectableRadioModel, the same harness group 3 below uses) and
    // seeds a value no live SignalAvg reading could plausibly produce, so
    // an ungated poll() would overwrite it with something near the
    // channel's actual reading instead of leaving it alone -- the
    // QCOMPARE below then fails loudly rather than passing by luck.
    // Verified by sabotage-and-revert: commenting out the
    // isTransmitting() early return in SliceMeterPump::poll() makes this
    // test fail (the seeded value gets overwritten); restoring the early
    // return makes it pass again. See task-12-report.md's fix-round
    // section for the exact before/after output.
    void pollDoesNothingWhileTransmitting()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        RxChannel* ch = model.wdspEngine()->rxChannel(slice->sliceIndex());
        QVERIFY(ch != nullptr);

        // A dBm S-meter reading this far out of any real WDSP range --
        // silence sits near -140, a strong signal maybe up to a few tens
        // of dB -- so the live channel could never coincidentally match
        // it. Confirmed explicitly rather than assumed, so a future
        // change to WDSP's meter scaling can't quietly turn this into a
        // vacuous pass.
        const double seeded = 12345.0;
        QVERIFY(ch->getMeter(RxMeterType::SignalAvg) + model.rxMeterOffsetDb() != seeded);

        // setSignalStrengthDbm is a plain public method (like setActive /
        // setTxSlice), not a QMetaProperty WRITE -- direct calls are the
        // normal way to seed it in a test.
        slice->setSignalStrengthDbm(seeded);
        slice->setSignalPeakDbm(seeded);
        slice->setSignalAverageDbm(seeded);

        model.radioStatus().setTransmitting(true);
        pollSettled(model, pump);

        QCOMPARE(slice->signalStrengthDbm(), seeded);
        QCOMPARE(slice->signalPeakDbm(), seeded);
        QCOMPARE(slice->signalAverageDbm(), seeded);

        harness.reset();
    }

    // ── Group 3: poll() against a real connected RxChannel ──────────────────

    // The default source (no selector ever wired) is SignalAverage, the
    // same fixed choice the pre-Task-12 pollSliceSMeters() made. Compares
    // against a read of the SAME live meter taken immediately after
    // poll() returns -- no intervening QTest::qWait or event-loop turn --
    // so the two reads observe WDSP's accumulator at, for all practical
    // purposes, the same instant.
    //
    // Known low-probability flake (fix round 1 review, Minor): this
    // QCOMPARE takes two independent getMeter() reads a few statements
    // apart on the SAME live channel; if the audio/DSP thread happens to
    // land a new block between them the accumulator can have moved by the
    // time the second read runs. The window is microseconds against a
    // ~21 ms block period at 48 kHz, so this is expected to be rare, not
    // wrong -- if this ever fails in isolation on an otherwise-unrelated
    // CI run, suspect this race FIRST, rerun once to confirm, and do not
    // treat a single such failure as evidence of a real regression here.
    void pollWritesSignalAverageByDefaultForAConnectedSlice()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        RxChannel* ch = model.wdspEngine()->rxChannel(slice->sliceIndex());
        QVERIFY(ch != nullptr);

        pollSettled(model, pump);

        const double expected =
            ch->getMeter(RxMeterType::SignalAvg) + model.rxMeterOffsetDb();
        QCOMPARE(slice->signalStrengthDbm(), expected);
        QCOMPARE(slice->signalPeakDbm(),
                 ch->getMeter(RxMeterType::SignalPeak) + model.rxMeterOffsetDb());
        QCOMPARE(slice->signalAverageDbm(), expected);

        harness.reset();
    }

    // Step 7: the rxMode()-driven source selector. Wiring the selector to
    // report SignalPeak must change which WDSP meter type gets read,
    // exactly matching MeterPoller::pollSMeter()'s SMeter/SMeterPeak
    // branch (RxMeterType::SignalPeak), not the fixed SignalAvg the old
    // pollSliceSMeters() always used.
    //
    // Carries the same low-probability two-independent-live-reads flake
    // pollWritesSignalAverageByDefaultForAConnectedSlice() documents above
    // -- if this fails in isolation, suspect that race before suspecting a
    // regression in the source-selector wiring itself.
    void pollWritesSignalPeakWhenSelectorSaysSo()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        RxChannel* ch = model.wdspEngine()->rxChannel(slice->sliceIndex());
        QVERIFY(ch != nullptr);

        pump->setSourceSelector([]() { return SliceMeterPump::MeterSource::SignalPeak; });
        pollSettled(model, pump);

        const double expected =
            ch->getMeter(RxMeterType::SignalPeak) + model.rxMeterOffsetDb();
        QCOMPARE(slice->signalStrengthDbm(), expected);
        QCOMPARE(slice->signalPeakDbm(), expected);
        QCOMPARE(slice->signalAverageDbm(),
                 ch->getMeter(RxMeterType::SignalAvg) + model.rxMeterOffsetDb());

        harness.reset();
    }

    // MaxBin, with setupMaxBinDetector() never called anywhere in this
    // test, is WdspEngine::getMaxBinDbm's own documented "not yet active"
    // case: an exact, deterministic -400.0 sentinel, passed straight
    // through per WdspEngine::getMaxBinDbm's doc comment and
    // MeterPoller::pollSMeter()'s own "> -400 else pass through unchanged"
    // handling. No live FFT or display channel needed, which is what
    // keeps this branch testable without a real waterfall running.
    void pollPassesThroughMaxBinNotActiveSentinel()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        QVERIFY(model.wdspEngine()->rxChannel(slice->sliceIndex()) != nullptr);
        QCOMPARE(model.wdspEngine()->getMaxBinDbm(/*disp=*/0), -400.0);

        pump->setSourceSelector([]() { return SliceMeterPump::MeterSource::MaxBin; });
        pollSettled(model, pump);

        QCOMPARE(slice->signalStrengthDbm(), -400.0);
        QCOMPARE(slice->signalPeakDbm(),
                 model.wdspEngine()->rxChannel(slice->sliceIndex())
                     ->getMeter(RxMeterType::SignalPeak) + model.rxMeterOffsetDb());
        QCOMPARE(slice->signalAverageDbm(),
                 model.wdspEngine()->rxChannel(slice->sliceIndex())
                     ->getMeter(RxMeterType::SignalAvg) + model.rxMeterOffsetDb());

        harness.reset();
    }

    // intervalMs()/setIntervalMs() clamp the same way MeterPoller's do
    // (Task 3.1 precedent, MeterPoller.cpp), so a persisted or live
    // MultimeterDelayMs of 0 (a stale/corrupt settings value) can never
    // stall the timer.
    void intervalMsClampsToTenTwoThousand()
    {
        RadioModel model{RadioModel::Role::Local};
        SliceMeterPump* pump = model.sliceMeterPump();
        QVERIFY(pump != nullptr);

        pump->setIntervalMs(0);
        QCOMPARE(pump->intervalMs(), 10);

        pump->setIntervalMs(50000);
        QCOMPARE(pump->intervalMs(), 2000);

        pump->setIntervalMs(250);
        QCOMPARE(pump->intervalMs(), 250);
    }

    // ── Group 4: construction is gated on Role ──────────────────────────────
    //
    // The obligation Task 5's brief step 1(e) could not discharge because
    // SliceMeterPump did not exist yet (see tst_remote_role_inert.cpp's own
    // header comment for the full history) -- reassigned to this task's
    // step 4b. RadioModel constructs WdspEngine unconditionally regardless
    // of role, so an unguarded pump would run a 10 Hz timer against a
    // channel-less engine on a Role::Remote model and clobber every
    // mirrored needle with the -140.0 fallback the moment a future task
    // wires the mirror's inbound apply into signalStrengthDbm.

    void localRoleConstructsSliceMeterPump()
    {
        RadioModel model{RadioModel::Role::Local};
        QVERIFY(model.sliceMeterPump() != nullptr);
    }

    void remoteRoleDoesNotConstructSliceMeterPump()
    {
        RadioModel model{RadioModel::Role::Remote};
        QVERIFY(model.sliceMeterPump() == nullptr);
    }
};

QTEST_MAIN(TestSliceMeterPump)
#include "tst_slice_meter_pump.moc"
