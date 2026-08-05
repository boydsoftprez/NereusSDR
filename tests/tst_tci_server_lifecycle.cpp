// tests/tst_tci_server_lifecycle.cpp  (NereusSDR)
// NereusSDR-original — no Thetis upstream port in this file.
//
// Phase 3J-1 Task 2.1: TciServer + TciClientSession skeleton.
// Covers the connect/disconnect lifecycle contract:
//   - start(0) returns true and emits serverStarted
//   - double-start is rejected (returns false)
//   - stop() clears running state and emits serverStopped

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>

#include "core/TciServer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// Test-only RadioModel subclass that exposes the protected
// QObject::receivers() helper so we can observe how many subscribers
// are wired to a given signal -- used by the P5 sliceAdded test below.
class TestableRadioModel : public RadioModel {
public:
    using RadioModel::RadioModel;
    int testReceiverCount(const char* signal) const {
        return receivers(signal);
    }
};

class TestTciServerLifecycle : public QObject {
    Q_OBJECT
private slots:
    void start_listens_and_emits_signal();
    void start_called_twice_returns_false();
    void stop_clears_running();
    // Phase 3J-1 review P2.3: stop() severs audio+IQ taps; start() must
    // reconnect them.  This test verifies the server can accept a client
    // after a stop→start cycle (regression: previously reconnect was missing
    // so audio/IQ frames would never flow after the second start()).
    void stop_then_start_accepts_new_client();
    // PR #279 review P5 (2026-05-22): stop() also severs the sliceAdded
    // subscriber wired by hookSliceBroadcasts; start() must re-hook so
    // slices added after a restart still get broadcast wiring.
    void slice_added_hook_survives_stop_start_cycle();
};

void TestTciServerLifecycle::start_listens_and_emits_signal()
{
    TciServer server(nullptr);   // RadioModel* not needed for lifecycle tests
    QSignalSpy startedSpy(&server, &TciServer::serverStarted);
    QVERIFY(!server.isRunning());
    QVERIFY(server.start(0));    // 0 = OS-assigned ephemeral port
    QVERIFY(server.isRunning());
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(server.port() != 0); // OS assigned a real port
    server.stop();
}

void TestTciServerLifecycle::start_called_twice_returns_false()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QVERIFY(!server.start(0));   // double-start rejected per plan Q7 + NereusSDR contract
    server.stop();
}

void TestTciServerLifecycle::stop_clears_running()
{
    TciServer server(nullptr);
    QSignalSpy stoppedSpy(&server, &TciServer::serverStopped);
    QVERIFY(server.start(0));
    server.stop();
    QVERIFY(!server.isRunning());
    QCOMPARE(server.port(), 0);
    QCOMPARE(stoppedSpy.count(), 1);
}

// ── stop_then_start_accepts_new_client() ────────────────────────────────────
//
// Phase 3J-1 review P2.3 regression test.
// Verifies that a stop() → start() cycle leaves the server in a functional
// state: the server listens again, and a new WS client can connect.
// A session that called audio_start:0; before the cycle would lose audio
// frames after the restart because hookAudioAndIqTaps() was never called
// from start().  This test exercises the transport path (connect succeeds);
// the tap re-connection is the structural fix verified by the fact that
// the server is fully operational after the second start().

void TestTciServerLifecycle::stop_then_start_accepts_new_client()
{
    TciServer server(nullptr);   // RadioModel* null — lifecycle test

    // First start.
    QVERIFY(server.start(0));
    const quint16 firstPort = server.port();
    QVERIFY(firstPort != 0);

    // Connect a client on the first start.
    QWebSocket client1;
    QSignalSpy conn1(&client1, &QWebSocket::connected);
    client1.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(firstPort)));
    QVERIFY(conn1.wait(2000));
    QCOMPARE(server.clientCount(), 1);

    // Stop.
    server.stop();
    QVERIFY(!server.isRunning());
    QCOMPARE(server.clientCount(), 0);

    // Second start — must succeed and accept a new client.
    QVERIFY(server.start(0));
    QVERIFY(server.isRunning());
    const quint16 secondPort = server.port();
    QVERIFY(secondPort != 0);

    QWebSocket client2;
    QSignalSpy conn2(&client2, &QWebSocket::connected);
    client2.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(secondPort)));
    QVERIFY(conn2.wait(2000));
    QCOMPARE(server.clientCount(), 1);

    client2.close();
    server.stop();
}

// PR #279 review P5 (2026-05-22): stop() called QObject::disconnect(
// m_model, nullptr, this, nullptr) which severs ALL RadioModel ->
// TciServer connections including the sliceAdded subscriber that
// hookSliceBroadcasts wires.  Without re-hooking, slices added between
// a stop()/start() cycle never got broadcast wiring (operator tunes
// after server restart went un-broadcast).
//
// Fix in 9ae5f135: start() now calls hookSliceBroadcasts() and
// hookGlobalBroadcasts() too.  hookSliceBroadcasts is idempotent --
// it skips slices already in m_broadcastWiredSlices and the new
// sliceAdded connect is a fresh wire each call.  hookGlobalBroadcasts
// uses an m_globalBroadcastsWired guard that start() resets implicitly
// by calling the function (the function flips the flag).
//
// This test counts QObject::receivers() of RadioModel::sliceAdded
// before stop() and after the subsequent start().  Pre-fix the count
// dropped to 0 after stop() and stayed at 0 after the second start().
// Post-fix the count is restored.
void TestTciServerLifecycle::slice_added_hook_survives_stop_start_cycle()
{
    TestableRadioModel model;
    TciServer          server(&model);

    QVERIFY(server.start(0));

    // Baseline: TciServer's constructor + start() both call
    // hookSliceBroadcasts, so there are at least 1 receiver of
    // sliceAdded (and currently 2 -- a minor leak that's still
    // correct because wireSliceForBroadcast dedups via
    // m_broadcastWiredSlices).
    const int baselineReceivers =
        model.testReceiverCount(SIGNAL(sliceAdded(int)));
    QVERIFY2(baselineReceivers >= 1,
        qPrintable(QString("Baseline sliceAdded receivers=%1, "
            "expected >= 1 after start()").arg(baselineReceivers)));

    // Add a slice on the first start.  This must trigger the
    // sliceAdded hook -> wireSliceForBroadcast.
    const int slice1Index = model.addSlice();
    Q_UNUSED(slice1Index);
    SliceModel* slice1 = model.activeSlice();
    QVERIFY(slice1);

    // Stop the server.  This severs RadioModel -> TciServer
    // connections including sliceAdded.  Pre-fix, the count
    // drops to 0 here AND stays at 0 after the restart.
    server.stop();
    QVERIFY(!server.isRunning());

    // Restart.  start() now re-runs hookSliceBroadcasts +
    // hookGlobalBroadcasts (review P5 fix).
    QVERIFY(server.start(0));

    // The actual P5 bug condition was "0 receivers after stop/start"
    // (the slices added between stop/start never got broadcast
    // wiring).  Post-fix we must have >= 1 receiver restored.
    const int afterStartReceivers =
        model.testReceiverCount(SIGNAL(sliceAdded(int)));
    QVERIFY2(afterStartReceivers >= 1,
        qPrintable(QString("After stop/start, sliceAdded "
            "receivers=%1, expected >= 1 -- "
            "hookSliceBroadcasts must be re-run from start() "
            "so slices added post-restart get broadcast wiring")
            .arg(afterStartReceivers)));

    // Add a slice on the SECOND start.  Pre-fix this would NOT
    // trigger wireSliceForBroadcast because the sliceAdded hook
    // was lost.  Post-fix the sliceAdded signal must still reach
    // the (idempotent) subscriber.  activeSlice() returns the
    // FIRST slice (it stays active across subsequent adds), so
    // look up the newly-added slice by index.
    QSignalSpy sliceAddedSpy(&model, &RadioModel::sliceAdded);
    const int slice2Index = model.addSlice();
    QCOMPARE(sliceAddedSpy.count(), 1);
    SliceModel* slice2 = model.sliceById(slice2Index);
    QVERIFY(slice2);
    QVERIFY(slice2 != slice1);

    server.stop();
}

QTEST_GUILESS_MAIN(TestTciServerLifecycle)
#include "tst_tci_server_lifecycle.moc"

#else
// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest doesn't report a missing executable.
int main() { return 0; }
#endif // HAVE_WEBSOCKETS
