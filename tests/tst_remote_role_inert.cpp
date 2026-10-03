// no-port-check: NereusSDR-original unit-test file. No Thetis logic is
// ported here; this exercises NereusSDR's own role/station-link seam
// (remote-daemon R2 Task 4).
// =================================================================
// tests/tst_remote_role_inert.cpp  (NereusSDR)
// =================================================================
//
// Remote-daemon R2 Task 4 -- RadioModel::Role, the station-link seam, and
// the remote-inert guard.
//
// A Role::Remote RadioModel is the shape a GUI-only process needs: no
// RadioConnection, no live WdspEngine channels, no running AudioEngine,
// because DSP and hardware I/O live in a daemon it talks to over the
// wire (R2 Task 18's wss session) instead of locally. The mechanism that
// makes that true is a single early return at the top of
// RadioModel::connectToRadio() (RadioModel.cpp) that fires when
// role() == Role::Remote, before RadioConnection creation, before
// WdspEngine::initialize(), before AudioEngine::start(), and before
// wireConnectionSignals() (which is what would otherwise construct
// RxDspWorker and wire every slice's WDSP connects) is ever called.
//
// The test below drives BOTH a Role::Local and a Role::Remote model
// through connectToRadio() and checks the same four things on each,
// asserting opposite outcomes. The Local arm is not a formality: a
// freshly constructed RadioModel already satisfies every assertion this
// test makes about the Remote model, because the constructor allocates
// AudioEngine and WdspEngine unconditionally (RadioModel.cpp) but starts
// neither. Without the Local arm actually reaching
// ConnectionState::Connected with all four things live, this test would
// pass against an empty/no-op implementation of the guard and prove
// nothing about it. See task-4-controller-notes.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-04 -- New test file for remote-daemon R2 Task 4. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-08-05 -- Extended with four assertions for remote-daemon R2
//                 Task 5 (the stream allocator, SliceModel's RADE
//                 reach-through, TxSliceArbiter, and the spot-collector
//                 auto-start restore -- design addendum section 4.1's
//                 "four local authorities"). J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    recordStreamVersion and the record
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/DxClusterClient.h"
#include "core/TxSliceArbiter.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/WsjtxClient.h"
#include "core/session/IStationLink.h"
#include "fakes/ConnectableRadioModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;

namespace {

// Trivial concrete IStationLink so attachStation()/detachStation() have
// something non-null to hold. Every verb refuses: this file's assertions
// are about a Role::Remote model being INERT, so a link that pretended to
// send would be the wrong stand-in. The command routing itself is covered
// by tst_remote_slice_commands.cpp against a real StationClient.
class NullStationLink : public NereusSDR::IStationLink {
public:
    ~NullStationLink() override = default;

    CommandOutcome requestAddSlice(const QString&) override { return refused(); }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return refused(); }
    CommandOutcome requestRemoveSlice(int) override { return refused(); }
    CommandOutcome requestActiveSlice(int) override { return refused(); }
    CommandOutcome requestSliceSampleRate(int, int) override { return refused(); }

private:
    static CommandOutcome refused()
    {
        return CommandOutcome{ false, QStringLiteral("NullStationLink sends nothing") };
    }
};

} // namespace

class TestRemoteRoleInert : public QObject {
    Q_OBJECT

private slots:
    // Pins that every existing construction site (MainWindow.cpp,
    // DaemonApp.cpp, and every test that writes plain `RadioModel model;`
    // or `new RadioModel(parent)`) keeps resolving to Role::Local without
    // having to change a single call site. Cheap and standalone --
    // doesn't need the ConnectableRadioModel harness at all.
    void defaultConstructorDefaultsToLocalRole()
    {
        RadioModel model;
        QCOMPARE(model.role(), RadioModel::Role::Local);

        // The new two-arg overload, called with Role::Local explicitly and
        // no parent -- exercises the constructor overload itself (distinct
        // code path from the one-arg constructor above) rather than
        // re-testing the same default a second time. Brace-init, not
        // parens: `RadioModel explicitLocal(RadioModel::Role::Local);`
        // parses as a most-vexing-parse function declaration instead of a
        // variable definition.
        RadioModel explicitLocal{RadioModel::Role::Local};
        QCOMPARE(explicitLocal.role(), RadioModel::Role::Local);
    }

    // The Local arm. Must reach Connected with a live RadioConnection, a
    // live RX WDSP channel, and a running AudioEngine -- the "all four
    // live" side of the brief's assertion pair. If this arm regressed
    // (e.g. the guard fired for Local too, or fired unconditionally),
    // ConnectableRadioModel::create() would time out and return nullptr,
    // and the QVERIFY below would catch it directly.
    void localReachesConnectedWithAllFourLive()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);

        RadioModel& model = harness->model();
        QCOMPARE(model.role(), RadioModel::Role::Local);
        QCOMPARE(model.connectionState(), ConnectionState::Connected);
        QVERIFY(model.connection() != nullptr);
        QVERIFY(model.wdspEngine()->rxChannel(0) != nullptr);
        QVERIFY(model.audioEngine()->isRunning());

        // Explicit teardown under this test's watch, same pattern
        // tst_connected_state_equivalence.cpp uses.
        harness.reset();
    }

    // The Remote arm. connectToRadio() must return inert: no
    // RadioConnection, connectionState() stuck at Disconnected, no RX
    // WDSP channel at any index, AudioEngine never started. Uses the
    // Task 2 harness extended with a Role parameter (this task), so both
    // arms of this test exercise the identical fake-radio wiring and
    // differ only in role -- the harness's create() call recognizes
    // Role::Remote and returns immediately after connectToRadio() rather
    // than waiting for a Connected transition that a correctly-guarded
    // Remote model will never reach.
    void remoteConnectToRadioIsInert()
    {
        std::unique_ptr<ConnectableRadioModel> harness =
            ConnectableRadioModel::create(10000, RadioModel::Role::Remote);
        QVERIFY(harness != nullptr);

        RadioModel& model = harness->model();
        QCOMPARE(model.role(), RadioModel::Role::Remote);
        QCOMPARE(model.connectionState(), ConnectionState::Disconnected);
        QVERIFY(model.connection() == nullptr);
        for (int n = 0; n < 8; ++n) {
            QVERIFY(model.wdspEngine()->rxChannel(n) == nullptr);
        }
        QVERIFY(!model.audioEngine()->isRunning());

        harness.reset();
    }

    // attachStation()/detachStation() hold a non-owning pointer the same
    // way m_spectrumSink does: settable, clearable, and safe to leave
    // attached across the model's destruction (RadioModel never deletes
    // it). Not exercised by connectToRadio() at all in this task -- that
    // wiring is a later task's job -- so this only pins that the seam
    // itself is present and inert.
    void attachAndDetachStationDoesNotOwnOrCrash()
    {
        RadioModel model{RadioModel::Role::Remote};
        NullStationLink link;

        model.attachStation(&link);
        model.detachStation();
        // Re-attach and let `model` be destroyed (end of scope) with the
        // link still attached, proving RadioModel does not delete it.
        model.attachStation(&link);
    }

    // =============================================================
    // Remote-daemon R2 Task 5 -- the four local authorities that keep
    // running behind a Role::Remote model's null connection and each
    // fight the daemon (design addendum
    // docs/architecture/2026-08-03-remote-daemon-r2-r3-design-addendum.md
    // section 4.1). "Null connection means inert" is false: these four
    // tests each drive one authority the way a future StateMirror
    // inbound-apply (task 8) will -- by calling the same SliceModel /
    // RadioModel entry point a local operator action uses -- and assert
    // it does nothing on a Role::Remote model.
    //
    // The brief this task was dispatched from listed a fifth assertion,
    // "SliceMeterPump is not constructed in Role::Remote". At the time
    // this file was written, that class did not exist anywhere in this
    // tree: it was task 12's own deliverable ("Create
    // src/core/meters/SliceMeterPump.{h,cpp}"), task 12's dependency line
    // named tasks 2 and 4 only (not this one), and task 12 step 4b was
    // where its own Role::Remote guard was specified. The design
    // addendum's own section 4.1 is titled "Four local authorities", not
    // five, and enumerates exactly the four covered below. Writing a test
    // (or a guard) for a class task 12 had not created yet would have
    // been fabricating both. See task-5-report.md for the full evidence
    // trail.
    //
    // Task 12 has since landed. The fifth assertion now lives in
    // tests/tst_slice_meter_pump.cpp -- localRoleConstructsSliceMeterPump()
    // and remoteRoleDoesNotConstructSliceMeterPump(), its own Group 4 --
    // not here, so this file's four tests below stay exactly the four
    // named above.
    // =============================================================

    // (a) The stream allocator. addSlice() wires an unconditional
    // frequencyChanged handler (RadioModel.cpp, inside addSlice) that
    // calls bindSliceToStream on every frequency change regardless of
    // role. Sizing the pool FIRST via the test-only
    // configureStreamPoolForTest seam matters: production
    // configureStreamPool no-ops for Role::Remote (this task's step 2),
    // so a test that never sizes the pool would already see
    // bindSliceToStream's pre-existing "unsized pool" guard return false
    // -- true with or without this task's role guard, proving nothing.
    // Sizing first removes that confound: with no role guard at all, an
    // unbound slice's first mirrored delta against a sized, empty pool
    // succeeds (NewStream), which visibly changes streamIndex from -1.
    void remoteMirroredFrequencyDeltaLeavesStreamBindingUntouched()
    {
        RadioModel model{RadioModel::Role::Remote};

        // addSliceWithStationId, not addSlice. On a Role::Remote model
        // addSlice() is now a WIRE VERB (RadioModel.h): it asks the
        // station for a slice and returns -1, because the id is the
        // station's to mint. addSliceWithStationId is the inbound
        // creator, and it reaches the same addSliceImpl body -- the same
        // unconditional frequencyChanged handler, the same
        // syncToSliceList -- so every assertion below is unchanged. The
        // same substitution is made in the two slots after this one.
        const int sliceId = model.addSliceWithStationId(0);
        QVERIFY(sliceId >= 0);
        SliceModel* slice = model.sliceById(sliceId);
        QVERIFY(slice != nullptr);

        // This ordering is LOAD-BEARING (review fix round 1, finding 2 --
        // an earlier version of this comment claimed the opposite).
        // addSlice() must run BEFORE the pool is sized, and stay that way:
        // sizing first and adding second would make addSlice()'s own
        // `poolReady` rollback branch see bindSliceToStream refuse (via
        // this task's role guard) against an already-sized pool, delete
        // the slice, and return -1, which fails the QVERIFY(sliceId >= 0)
        // above outright. Reversing these two lines is a direct way to
        // watch that failure. Added in this order, the slice is created
        // while the pool is still unsized -- the pre-existing "Slice A
        // must survive" path -- and only afterwards does the pool get
        // sized via the test-only bypass, leaving the slice unbound
        // (streamIndex -1) for the mirrored-delta assertion below to act
        // on.
        model.configureStreamPoolForTest(2, 2, 192000);

        const int    streamIndexBefore = slice->streamIndex();
        const double shiftBefore       = slice->shiftOffsetHz();
        const int    rateBefore        = slice->sampleRateHz();

        QSignalSpy retuneRejectedSpy(&model, &RadioModel::sliceRetuneRejected);

        // The mirrored delta: SliceModel::setFrequency is the same entry
        // point a local VFO drag uses, and it is what a future StateMirror
        // inbound-apply will call to reflect the daemon's own retune.
        slice->setFrequency(slice->frequency() + 100000.0);

        QCOMPARE(slice->streamIndex(), streamIndexBefore);
        QCOMPARE(slice->shiftOffsetHz(), shiftBefore);
        QCOMPARE(slice->sampleRateHz(), rateBefore);
        // Not independently discriminating in this specific scenario: the
        // slice was never bound, so the frequencyChanged handler returns
        // on `previousStream < 0` before the emit in both the guarded and
        // the do-nothing case, and this QCOMPARE would pass either way.
        // Kept as a regression guard on the assertion as a whole, not
        // relied on alone -- the three field comparisons above are what
        // actually fail against a do-nothing implementation.
        QCOMPARE(retuneRejectedSpy.count(), 0);
    }

    // (b) SliceModel's one reach-through into the DSP engine.
    // setDspMode does qobject_cast<RadioModel*>(parent()) and, on entry
    // into a RADE sideband, calls WdspEngine::createRadeChannel, which
    // carries no isInitialized guard (WdspEngine.cpp:685: "no
    // m_initialized requirement"). A Role::Remote model's WdspEngine is
    // constructed but never initialize()'d (task 4), so without a role
    // guard here this would construct and start() a live RadeChannel --
    // a real vocoder -- on a machine with no DSP role at all.
    void remoteMirroredRadeModeDoesNotCreateRadeChannel()
    {
        RadioModel model{RadioModel::Role::Remote};

        const int sliceId = model.addSliceWithStationId(0);  // see (a) above
        QVERIFY(sliceId >= 0);
        SliceModel* slice = model.sliceById(sliceId);
        QVERIFY(slice != nullptr);

        QVERIFY(model.wdspEngine()->radeChannel(slice->sliceIndex()) == nullptr);

        // The mirrored transition: a future StateMirror inbound-apply
        // reflects the daemon's dspMode the same way a local mode-button
        // press does, through this same setter.
        slice->setDspMode(DSPMode::RADE_U);

        QVERIFY(model.wdspEngine()->radeChannel(slice->sliceIndex()) == nullptr);
    }

    // (c) TxSliceArbiter. addSlice() calls
    // TxSliceArbiter::syncToSliceList() unconditionally, and on the
    // first slice added (nothing yet flagged) its "initial bind" arm
    // calls SliceModel::setTxSlice(true) -- which slice transmits is the
    // daemon's decision, mirrored in later than this task, not a value a
    // Role::Remote client should compute for itself.
    void remoteAddSliceDoesNotClaimTxSlice()
    {
        RadioModel model{RadioModel::Role::Remote};

        const int sliceId = model.addSliceWithStationId(0);  // see (a) above
        QVERIFY(sliceId >= 0);
        SliceModel* slice = model.sliceById(sliceId);
        QVERIFY(slice != nullptr);

        QVERIFY(!slice->isTxSlice());
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), -1);
    }

    // (d) The spot collectors. restoreSpotClientAutoStartState() is
    // called once at GUI startup (MainWindow.cpp:771) with no dependence
    // on connection state, so a Role::Remote GUI process would otherwise
    // dial every auto-start-enabled spot source itself -- duplicating
    // whatever the daemon also does, including logging into the same DX
    // cluster and uploading to PSK Reporter under one callsign (design
    // addendum risk 9). Proven WITHOUT touching the network: DxCluster /
    // RBN point at 127.0.0.1 on a port nothing listens on, so the Local
    // arm's connect attempt is refused on loopback (no packet ever
    // leaves the machine) and observed via connectionError; WSJT-X binds
    // a local UDP socket, observed via isListening(). See
    // task-5-controller-notes.md "Step 5, the spot collectors" -- POTA is
    // deliberately not exercised here because PotaClient::startPolling()
    // fires an immediate synchronous poll against the real
    // api.pota.app, which this task's "do not touch the network"
    // constraint rules out; the three sources below already prove the
    // gate is a single whole-function guard, not a per-source one.
    void remoteRestoreSpotClientAutoStartStateStaysSilent()
    {
        auto& settings = AppSettings::instance();
        settings.clear();

        settings.setValue("DxClusterAutoConnect", "True");
        settings.setValue("DxClusterHost", "127.0.0.1");
        settings.setValue("DxClusterPort", 18291);
        settings.setValue("DxClusterCallsign", "KG4VCF");

        settings.setValue("RbnAutoConnect", "True");
        settings.setValue("RbnHost", "127.0.0.1");
        settings.setValue("RbnPort", 18292);
        settings.setValue("RbnCallsign", "KG4VCF");

        settings.setValue("WsjtxAutoStart", "True");
        settings.setValue("WsjtxAddress", "127.0.0.1");
        settings.setValue("WsjtxPort", 28291);

        {
            // Local arm: the restore must still run for real -- proves
            // the guard this task adds is role-gated, not a blanket
            // no-op that would silently regress local direct mode.
            RadioModel local{RadioModel::Role::Local};
            QSignalSpy dxErrorSpy(local.dxCluster(),
                                  &DxClusterClient::connectionError);
            QSignalSpy rbnErrorSpy(local.rbn(),
                                   &DxClusterClient::connectionError);

            local.restoreSpotClientAutoStartState();

            QVERIFY(QTest::qWaitFor(
                [&]() { return dxErrorSpy.count() > 0; }, 3000));
            QVERIFY(QTest::qWaitFor(
                [&]() { return rbnErrorSpy.count() > 0; }, 3000));
            QVERIFY(local.wsjtx()->isListening());
        }

        {
            // Remote arm: no cluster login -- no connect attempt (so no
            // connectionError, ever, not even a refused one). Its own
            // WSJT-X listener binds (parity Task 19).
            RadioModel remote{RadioModel::Role::Remote};
            QSignalSpy dxErrorSpy(remote.dxCluster(),
                                  &DxClusterClient::connectionError);
            QSignalSpy rbnErrorSpy(remote.rbn(),
                                   &DxClusterClient::connectionError);

            remote.restoreSpotClientAutoStartState();

            // Give a wrongly-not-guarded implementation's async connect
            // time to fail, so absence is observed, not merely unproven
            // yet.
            QTest::qWait(500);

            QCOMPARE(dxErrorSpy.count(), 0);
            QCOMPARE(rbnErrorSpy.count(), 0);
            QVERIFY(!remote.dxCluster()->isConnected());
            QVERIFY(!remote.rbn()->isConnected());
            // Parity Task 19 (R-IOS-25): WSJT-X listens for programs on the
            // computer it runs on, so a remote window starts its own; the
            // Core runs the cluster and RBN (tst_remote_spots).
            QVERIFY(remote.wsjtx()->isListening());
            remote.wsjtx()->stopListening();
        }

        settings.clear();
    }
};

QTEST_MAIN(TestRemoteRoleInert)
#include "tst_remote_role_inert.moc"
