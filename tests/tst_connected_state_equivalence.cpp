// no-port-check: NereusSDR-original unit-test file. No Thetis logic is
// ported here; this exercises NereusSDR's own connection-state storage
// seam (remote-daemon R2 Task 3).
// =================================================================
// tests/tst_connected_state_equivalence.cpp  (NereusSDR)
// =================================================================
//
// Remote-daemon R2 Task 3 -- make connected state storage-backed.
//
// RadioModel::isConnected() used to derive from the connection pointer
// (m_connection && m_connection->isConnected()), so a remote client that
// deliberately owns no RadioConnection (the shape a StateMirror-driven GUI
// will be in from R2 onward) would report disconnected forever --
// RadioModel::maxSlices() short-circuits on it and returns 1, so a remote
// client would believe every station is single-slice regardless of what it
// actually advertised. See docs/architecture/
// 2026-08-03-remote-daemon-r2-r3-design-addendum.md section 5.
//
// isConnected() is now `m_connectionState == ConnectionState::Connected`
// (RadioModel.cpp), a stored member that was already the single source of
// truth the UI reads via connectionState() (RadioModel.h). This file
// proves the new derivation agrees with the old pointer-derived one across
// a REAL connect and teardown (via tests/fakes/ConnectableRadioModel,
// remote-daemon R2 Task 2), not just when sampled before and after: it
// also observes the disconnect transition from inside teardownConnection(),
// which is the one place the two derivations were able to genuinely
// diverge before this task (design addendum section 5's window:
// teardownWorkerThreadedConnection used to null m_connection 24 lines
// before setConnectionState(Disconnected) ran, so isConnected() would have
// read stale-Connected while m_connection was already gone -- exactly the
// state RadioModel::setTune()'s power-on guard checks before letting TUNE
// proceed).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-04 -- New test file for remote-daemon R2 Task 3. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QScopeGuard>
#include <QSignalSpy>

#include <memory>

#include "core/ConnectionState.h"
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "fakes/ConnectableRadioModel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;

class TestConnectedStateEquivalence : public QObject {
    Q_OBJECT

private slots:
    // Core proof for this task: isConnected() (now m_connectionState
    // backed) and the old pointer-derived expression, (model.connection()
    // && model.connection()->isConnected()), must agree at every edge a
    // black-box test can reach across a REAL connect and teardown cycle,
    // not just when sampled before and after. A before-and-after test
    // would still pass even if a stale-Connected window remained inside
    // teardownConnection(), because both derivations already agree
    // trivially at rest: before connecting, m_connection is null and
    // m_connectionState is Disconnected; after a completed teardown, both
    // are back to that same rest state. The edge worth watching is the one
    // in between.
    void agreesAcrossRealConnectAndTeardown()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);

        RadioModel& model = harness->model();

        // ---- Edge 1: steady Connected state, before teardown starts ----
        QCOMPARE(model.connectionState(), ConnectionState::Connected);
        QVERIFY(model.connection() != nullptr);
        QVERIFY(model.connection()->isConnected());
        QVERIFY(model.isConnected());
        QCOMPARE(model.isConnected(),
                 static_cast<bool>(model.connection() && model.connection()->isConnected()));

        // ---- Edge 2: the disconnect transition itself, observed from
        // inside teardownConnection() via a same-thread (therefore direct,
        // synchronous) connection to connectionStateChanged. ----
        //
        // RadioModel::teardownConnection() forces m_connectionState to
        // Disconnected BEFORE teardownWorkerThreadedConnection nulls
        // m_connection and dispatches the real protocol-level disconnect()
        // onto the connection's worker thread (this task's ordering fix).
        // Connecting here observes the model at exactly that instant.
        //
        // Two things are checked, not one:
        //   (a) isConnected() already reads false. This holds by
        //       construction no matter where the forcing call sits, because
        //       setConnectionState() assigns m_connectionState before it
        //       emits, so it is a sanity check rather than the load-bearing
        //       assertion.
        //   (b) connection() is STILL NON-NULL here. This is the
        //       load-bearing assertion: it is what "hoisted ahead of the
        //       nulling" means operationally. Before this task's ordering
        //       fix, setConnectionState(Disconnected) ran well after
        //       teardownWorkerThreadedConnection had already nulled
        //       m_connection, so this same assertion would have observed
        //       connection() == nullptr here.
        //
        // This handler deliberately does NOT also assert
        // model.connection()->isConnected() == model.isConnected() at this
        // exact instant, because that is not expected to hold, and that is
        // correct rather than a gap: the underlying RadioConnection's own
        // protocol-level state (its std::atomic<ConnectionState> m_state,
        // flipped by P1RadioConnection::disconnect() -> setState) has not
        // changed yet. That call happens moments later, dispatched onto the
        // connection's worker thread from inside
        // teardownWorkerThreadedConnection, which runs AFTER this signal.
        // So connection()->isConnected() still reads true here even though
        // the model has already, deliberately, started reporting
        // Disconnected: the model is intentionally conservative (it flips
        // its externally-visible state slightly ahead of the wire-level
        // handshake completing) rather than the dangerous alternative of
        // claiming Connected after the connection is already gone.
        // Captured below for the record, not asserted equal to
        // isConnected().
        bool sawDisconnectedSignal = false;
        bool connectionStillNonNullAtSignal = false;
        bool modelReportedDisconnectedAtSignal = false;
        bool connectionObjectStillClaimedConnectedAtSignal = false;

        connect(&model, &RadioModel::connectionStateChanged, &model,
                [&](ConnectionState state) {
            if (state != ConnectionState::Disconnected) {
                return;
            }
            sawDisconnectedSignal = true;
            connectionStillNonNullAtSignal = (model.connection() != nullptr);
            modelReportedDisconnectedAtSignal = !model.isConnected();
            connectionObjectStillClaimedConnectedAtSignal =
                (model.connection() != nullptr) && model.connection()->isConnected();
        });

        QSignalSpy disconnectSpy(&model, &RadioModel::connectionStateChanged);

        model.disconnectFromRadio();

        QCOMPARE(disconnectSpy.count(), 1);
        QVERIFY2(sawDisconnectedSignal,
                 "connectionStateChanged(Disconnected) never fired; this "
                 "test proves nothing about the teardown ordering");
        QVERIFY2(connectionStillNonNullAtSignal,
                 "m_connection was already null when the model reported "
                 "Disconnected: setConnectionState(Disconnected) must run "
                 "before teardownWorkerThreadedConnection nulls the "
                 "connection, not after");
        QVERIFY(modelReportedDisconnectedAtSignal);
        // Diagnostic only; see the long comment above for why this is
        // expected to be true rather than a bug.
        QVERIFY(connectionObjectStillClaimedConnectedAtSignal);

        // ---- Edge 3: steady Disconnected state, after teardown completes ----
        QCOMPARE(model.connectionState(), ConnectionState::Disconnected);
        QVERIFY(model.connection() == nullptr);
        QVERIFY(!model.isConnected());
        QCOMPARE(model.isConnected(),
                 static_cast<bool>(model.connection() && model.connection()->isConnected()));

        // Explicit teardown rather than letting `harness` fall out of scope,
        // so RadioModel::~RadioModel() and P1FakeRadio's destructor both run
        // here, under this test's watch. teardownConnection() early-returns
        // on the now-null m_connection, so this is a safe no-op for the
        // connection itself and only tears down the fake.
        harness.reset();
    }

    // Direct proof of the R2 motivation (design addendum section 5): a
    // RadioModel with NO RadioConnection object at all, the shape a
    // remote-daemon client is in once a later task drives m_connectionState
    // from the wire instead of from a local connect, must still be able to
    // report isConnected() == true. Before this task, isConnected() was
    // `m_connection && m_connection->isConnected()`, so a null m_connection
    // made isConnected() permanently false regardless of m_connectionState.
    void isConnectedNoLongerRequiresAConnectionObject()
    {
        RadioModel model;
        QVERIFY(model.connection() == nullptr);
        QVERIFY(!model.isConnected());

        model.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model.connection() == nullptr);  // still no connection object
        QVERIFY(model.isConnected());            // yet reports connected

        model.setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(!model.isConnected());
    }

    // Step 4 of this task: injectConnectionForTest must also drive
    // m_connectionState, or the 24 test files that use it as a
    // real-connection stand-in would silently desync from the now
    // storage-backed isConnected(). Two of them actually depend on it:
    // tests/tst_p2_ddc_assignment_marshalling.cpp and
    // tests/tst_p2_ddc_mask_ownership.cpp both drive the isConnected() gate
    // on the P2 DDC wire push at RadioModel.cpp:15412 through this seam.
    // This test pins the seam's contract directly, independent of that
    // wire-push behavior.
    void injectConnectionForTestDrivesConnectionState()
    {
        RadioModel model;
        QCOMPARE(model.connectionState(), ConnectionState::Disconnected);
        QVERIFY(!model.isConnected());

        // A stack-local P1RadioConnection that never has init() or
        // moveToThread() called on it is a safe, valid, non-null
        // RadioConnection* to inject: init() (which creates sockets and
        // timers) is documented as needing to run on the worker thread
        // after moveToThread(), and this object never leaves the test's
        // thread, so there is nothing for its destructor to tear down
        // across threads. Same pattern tests/tst_p2_ddc_mask_ownership.cpp
        // already relies on for P2RadioConnection.
        P1RadioConnection conn(nullptr);

        model.injectConnectionForTest(&conn);
        // Detach before `conn` goes out of scope however this slot is left,
        // including an early return from a failed QCOMPARE/QVERIFY above:
        // RadioModel::~RadioModel() runs teardownConnection() unconditionally
        // when m_connection is still non-null, and local destruction order
        // is reverse of declaration, so `conn` (declared after `model`)
        // would otherwise be destroyed first, leaving m_connection dangling
        // when model's own destructor tries to use it. Same guard
        // tests/tst_radio_model_drive_path.cpp uses for the same reason.
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        QCOMPARE(model.connectionState(), ConnectionState::Connected);
        QVERIFY(model.isConnected());

        model.injectConnectionForTest(nullptr);
        QCOMPARE(model.connectionState(), ConnectionState::Disconnected);
        QVERIFY(!model.isConnected());
    }
};

QTEST_MAIN(TestConnectedStateEquivalence)
#include "tst_connected_state_equivalence.moc"
